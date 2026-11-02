#!/usr/bin/env python3
"""Native layout regression checks; no engine rendering or input emulation."""
from pathlib import Path
import unittest
from layout_model import parse_layout, extent

SOURCE = Path(__file__).resolve().parents[1] / 'Source/GorillaMap/GUI/layouts'


class MapLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = parse_layout(SOURCE / 'main.layout')

    def test_overlay_layers_are_explicitly_above_fullscreen_map(self):
        base = self.root.find('Map')
        self.assertEqual(extent(base, 1920, 1080), (1920, 1080))
        for name in ('ZoneCanvas', 'PlayerLabel', 'Sidebar', 'ExpandSidebarFrame', 'WorldBadge', 'Compass', 'InfoCard'):
            with self.subTest(widget=name):
                self.assertIn('priority', self.root.find(name).properties)
                self.assertGreater(self.root.find(name).number('priority'), base.number('priority'))
        for name in ('Editor', 'Groups', 'Settings'):
            with self.subTest(modal=name):
                self.assertGreater(self.root.find(name).number('priority'), self.root.find('Sidebar').number('priority'))

    def test_fullscreen_passive_overlays_do_not_consume_pointer(self):
        self.assertEqual(self.root.find('ZoneCanvas').number('ignorepointer'), 1)
        self.assertEqual(self.root.find('PlayerLabel').number('ignorepointer'), 1)
        self.assertEqual(self.root.find('Map').number('ignorepointer'), 0)
        self.assertEqual(self.root.find('Map').number('disabled'), 0)

    def test_menu_starts_with_sidebar_and_without_blocking_modals(self):
        self.assertEqual(self.root.find('Sidebar').number('visible'), 1)
        for name in ('Editor', 'Groups', 'Settings', 'ExpandSidebarFrame'):
            with self.subTest(widget=name):
                self.assertEqual(self.root.find(name).number('visible'), 0)
        self.assertEqual(self.root.find('CreateMarker').number('disabled'), 0)
        self.assertEqual(self.root.find('CloseMap').number('disabled'), 0)

    def test_sidebar_and_information_card_fit_supported_viewports(self):
        for width, height in ((1280,720),(1673,940),(1920,1080),(2560,1440),(3440,1440)):
            for name in ('Sidebar','WorldBadge','Compass','InfoCard'):
                node=self.root.find(name)
                pos=node.properties['position']
                w,h=extent(node,width,height)
                x,y=float(pos[0])*width,float(pos[1])*height
                with self.subTest(viewport=(width,height),widget=name):
                    self.assertGreaterEqual(x,0)
                    self.assertGreaterEqual(y,0)
                    self.assertLessEqual(x+w,width+1)
                    self.assertLessEqual(y+h,height+1)

    def test_marker_actions_are_foreground_and_clickable(self):
        row=parse_layout(SOURCE / 'marker_row.layout')
        background=row.find('RowBackground')
        self.assertEqual(background.number('ignorepointer'),1)
        for name in ('RowSelect','RowVisibility','RowEdit','RowDelete'):
            with self.subTest(widget=name):
                action=row.find(name)
                self.assertEqual(action.number('disabled'),0)
                self.assertEqual(action.number('ignorepointer'),0)
                self.assertGreater(action.number('priority'),background.number('priority'))


if __name__=='__main__':
    unittest.main(verbosity=2)
