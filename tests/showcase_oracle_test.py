#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Prove the room oracle rejects changed state and synthetic damaged readbacks."""
import copy
import struct
import unittest
from render_test_support import CHECKS, Failure
from showcase_oracle import expected_state, state, expected_pixel, pixels, COLORS, CLEAR


class OracleTests(unittest.TestCase):
    def rejects(self,call):
        saved=len(CHECKS)
        try:
            with self.assertRaises(Failure):call()
        finally:
            del CHECKS[saved:]

    def test_rollback_and_identity(self):
        expected=expected_state(3)
        state(expected,3,'control')
        for field,value in (('entity_uuid','00000007-0000-4000-8000-000000000099'),('generation',2),('retired',True)):
            damaged=copy.deepcopy(expected);damaged['slots'][3][field]=value
            self.rejects(lambda:state(damaged,3,'reject identity corruption'))
        damaged=copy.deepcopy(expected)
        damaged['slots'][3]['entity']['transform']['position_m']=[3.1,-.55,.15]
        self.rejects(lambda:state(damaged,3,'reject leaked valid prefix'))
        damaged=expected_state(4);damaged['slots'][4]['entity']['authoring_revision']=3
        self.rejects(lambda:state(damaged,4,'reject stale entity revision'))

    def test_pixels(self):
        # Fixed camera rays: background, dark back wall and unchanged side wall.
        self.assertEqual(expected_pixel(0,0,320,240,3)[:2],(0,0))
        self.assertEqual(expected_pixel(270,100,320,240,3)[:2],(2,6))
        self.assertEqual(expected_pixel(213,120,320,240,3)[:2],(3,5))
        for revision in (3,4):
            expected=[expected_pixel(x,y,320,240,revision) for y in range(240) for x in range(320)]
            color=bytes(c for identity,face,d in expected for c in (COLORS[face-1] if identity else CLEAR))
            ids=b''.join(struct.pack('<I',i) for i,f,d in expected)
            depth=b''.join(struct.pack('<f',d) for i,f,d in expected)
            pixels(color,ids,depth,320,240,revision,'R8G8B8A8_UNORM')
            self.rejects(lambda:pixels(color,bytes(len(ids)),depth,320,240,revision,'R8G8B8A8_UNORM'))
            self.rejects(lambda:pixels(bytes(len(color)),ids,depth,320,240,revision,'R8G8B8A8_UNORM'))
            self.rejects(lambda:pixels(color,ids,b''.join(struct.pack('<f',1.) for _ in expected),320,240,revision,'R8G8B8A8_UNORM'))


if __name__=='__main__':
    unittest.main()
