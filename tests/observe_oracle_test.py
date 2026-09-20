#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
import copy
import re
from observe_oracle import expected, validate, Failure, CHECKS, ROOT, strict_json


def main():
    schema=strict_json((ROOT/"schemas/protocol-0.1.schema.json").read_bytes())
    assert {"$ref":"#/$defs/tagsSet"} in schema["properties"]["operations"]["items"]["oneOf"]
    operation=schema["$defs"]["tagsSet"]
    assert operation["type"]=="object" and operation["additionalProperties"] is False
    assert set(operation["required"])==set(operation["properties"])=={"type","target","tags"}
    assert operation["properties"]["type"]=={"const":"entity.tags.set"}
    assert operation["properties"]["target"]=={"oneOf":[{"$ref":"#/$defs/temporaryTarget"},{"$ref":"#/$defs/durableTarget"}]}
    assert operation["properties"]["tags"]=={"type":"array","maxItems":8,"uniqueItems":True,
        "items":{"type":"string","pattern":r"^[A-Za-z0-9][A-Za-z0-9._:-]{0,31}(?![\s\S])"}}
    tag_pattern=operation["properties"]["tags"]["items"]["pattern"]
    for value in ("red", "A.b_c:2-", "x"*32): assert re.search(tag_pattern,value)
    for value in ("", "red\n", "red\r\n", "a b", "x"*33): assert not re.search(tag_pattern,value)
    original=expected();validate(original)
    paths=[("records",0,"page","items",0,"entity_uuid"),
           ("records",0,"page","items",1,"bounds","lower",0),
           ("records",1,"page","world_revision"),
           ("records",2,"page","items",0,"generation"),
           ("records",3,"page","has_more"),
           ("records",4,"code"),
           ("records",13,"page","items",0,"tags",0),
           ("records",14,"code"),("state_unchanged_by_queries",)]
    cases=[]
    for path in paths:
        bad=copy.deepcopy(original);node=bad
        for key in path[:-1]:node=node[key]
        old=node[path[-1]];node[path[-1]]=not old if type(old) is bool else "corrupt" if type(old) is str else old+1
        cases.append(bad)
    bad=copy.deepcopy(original);bad["records"][0]["page"]["items"][0]["parent"]="hidden";cases.append(bad)
    bad=copy.deepcopy(original);bad["records"][8]["page"]["items"]=[original["records"][0]["page"]["items"][0]];cases.append(bad)
    bad=copy.deepcopy(original);bad["records"][0]["page"]["items"].reverse();cases.append(bad)
    bad=copy.deepcopy(original);bad["records"][0]["page"]["world_revision"]=True;cases.append(bad)
    for bad in cases:
        try:validate(bad)
        except Failure:pass
        else:raise RuntimeError("corrupt query proof was accepted")
    CHECKS.clear();print("query oracle selftest passed: "+str(len(cases))+" corruptions");return 0


if __name__=="__main__":raise SystemExit(main())
