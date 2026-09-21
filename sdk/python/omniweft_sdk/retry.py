# SPDX-License-Identifier: Apache-2.0
"""Explicit opt-in bounded retries. Never generates a replacement for an uncertain key."""
from __future__ import annotations
from dataclasses import dataclass, field
import json
import uuid as uuid_module
from .client import Capabilities, ERROR_CODES
from .models import (CreateCube, DeleteEntity, SetTransform, OutcomeUnknown, ProtocolError,
                     Receipt, UINT64_MAX, decode, fields, integer, uuid)
from .policy import PolicyClient, PolicySession


@dataclass(frozen=True)
class RetryLimits:
    receipts_per_principal: int
    canonical_payload_bytes: int
    receipt_field_bytes: int
    receipt_ttl_ms: int
    principal_count: int


@dataclass(frozen=True)
class RetryCapabilities(Capabilities):
    retry_limits: RetryLimits

    @classmethod
    def from_dict(cls, value: object) -> RetryCapabilities:
        v=fields(value,set(Capabilities.__dataclass_fields__)|{"retry_limits"})
        if v["retry_mode"]!="retained_receipts_v1": raise ProtocolError()
        limits=fields(v["retry_limits"],set(RetryLimits.__dataclass_fields__))
        for name,number in zip(RetryLimits.__dataclass_fields__,(4,16384,8192,2000,2)):
            integer(limits[name],number,number)
        legacy={k:x for k,x in v.items() if k!="retry_limits"}
        legacy["retry_mode"]="resync_only"
        base=Capabilities.from_dict(legacy)
        return cls(**{**base.__dict__,"retry_mode":"retained_receipts_v1","retry_limits":RetryLimits(**limits)})


@dataclass(frozen=True)
class PreparedTransaction:
    sequence: int
    transaction_id: str
    expected_revision: int
    _owner: object = field(repr=False,compare=False)
    _body: bytes = field(repr=False)


class RetryPolicyClient(PolicyClient):
    _error_codes=ERROR_CODES|{"IDEMPOTENCY_MISMATCH"}

    def __init__(self,info,principal):
        super().__init__(info,principal)
        self._request_owner=object()
        self._pending: PreparedTransaction | None=None

    def capabilities(self) -> RetryCapabilities:
        with self._lock:
            caps=RetryCapabilities.from_dict(self._request("GET","/v0/capabilities"))
            if caps.epoch!=self._info.epoch or caps.limits.session_ttl_ms!=self._info.session_ttl_ms:
                raise ProtocolError()
            if self._next is not None and caps.next_sequence<self._next: raise ProtocolError()
            self._caps,self._next=caps,caps.next_sequence
            return caps

    def prepare(self,operations,expected_revision,*,transaction_id=None) -> PreparedTransaction:
        """Snapshot one request. Keep this handle to explicitly retry after uncertainty."""
        with self._lock:
            if self._uncertain or self._pending is not None: raise OutcomeUnknown()
            caps=self._ensure_negotiated()
            integer(expected_revision)
            if not 1<=len(operations)<=4 or any(type(op) not in (CreateCube,SetTransform,DeleteEntity) for op in operations):
                raise ProtocolError()
            if self._next is None or self._next>=UINT64_MAX:
                self._uncertain=True
                raise OutcomeUnknown()
            identifier=uuid(str(uuid_module.uuid4()) if transaction_id is None else transaction_id)
            body={"protocol_version":"0.1","world_id":caps.world_id,"transaction_id":identifier,
                  "idempotency":{"epoch":self._info.epoch,"sequence":self._next},
                  "expected_world_revision":expected_revision,
                  "apply_at":{"mode":"next_tick","expires_after_ticks":120},
                  "budget":{"max_operations":len(operations),"max_blob_bytes":0},
                  "operations":[op.to_dict() for op in operations]}
            try: payload=json.dumps(body,separators=(",",":"),ensure_ascii=False,allow_nan=False).encode("utf-8")
            except (ValueError,TypeError,OverflowError,UnicodeError): raise ProtocolError() from None
            if len(payload)>caps.retry_limits.canonical_payload_bytes: raise ProtocolError()
            self._pending=PreparedTransaction(self._next,identifier,expected_revision,self._request_owner,payload)
            return self._pending

    def submit(self,prepared: PreparedTransaction) -> Receipt:
        """Submit/replay this exact request, including after an unknown response."""
        with self._lock:
            if type(prepared) is not PreparedTransaction or prepared._owner is not self._request_owner:
                raise ProtocolError()
            if self._pending is not None and self._pending is not prepared: raise OutcomeUnknown()
            caps=self._ensure_negotiated()
            self._pending=prepared
            value=self._request("POST","/v0/transactions",decode(prepared._body),mutating=True)
            try:
                wrapper=self._wrapper(value,"receipt")
                receipt=Receipt.from_dict(wrapper["receipt"])
                if (wrapper["next_sequence"]<prepared.sequence+1 or receipt.transaction_id!=prepared.transaction_id
                        or any(binding.world_id!=caps.world_id for binding in receipt.created)):
                    raise ProtocolError()
                self._next=wrapper["next_sequence"]
                self._pending=None
                self._uncertain=False
                return receipt
            except ProtocolError:
                self._uncertain=True
                raise OutcomeUnknown() from None

    def transact(self,operations,expected_revision,*,transaction_id=None) -> Receipt:
        return self.submit(self.prepare(operations,expected_revision,transaction_id=transaction_id))

    def resync(self):
        """Explicit caller reconciliation abandons pending retry state after observing."""
        with self._lock:
            snapshot=super().resync()
            self._pending=None
            return snapshot

    def resync_policy(self):
        """Explicit recovery when the full observation exceeds the existing grant."""
        with self._lock:
            status=super().resync_policy()
            self._pending=None
            return status


class RetryPolicySession(PolicySession):
    _profile="policy.retry.v1"

    def client(self,principal="west") -> RetryPolicyClient:
        with self._lock:
            if self._process is None or self._process.poll() is not None or principal not in self._principals:
                raise ProtocolError()
            return RetryPolicyClient(self._principals[principal],principal)
