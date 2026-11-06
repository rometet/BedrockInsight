import unittest
from scripts.audit_lk_parity import PROJECT,custom_keys,inventory


class ParityInventoryTests(unittest.TestCase):
    def setUp(self):
        self.requirements=(PROJECT/'LK_STATS_FULL_PARITY_REQUIREMENTS.md').read_text(encoding='utf-8-sig')
        self.catalog=(PROJECT/'src/bedrock_insight/core/stats_catalog.hpp').read_text(encoding='utf-8-sig')
        self.matrix=(PROJECT/'docs/LK_FULL_PARITY_MATRIX.md').read_text(encoding='utf-8-sig')

    def test_all_mandatory_rows_exist_and_none_accepted_from_enum_or_smoke(self):
        report=inventory(self.requirements,self.catalog,self.matrix)
        self.assertEqual(len(report['rows']),96)
        self.assertEqual(sum(x['group']=='custom' for x in report['rows']),77)
        self.assertEqual(sum(x['group']=='category' for x in report['rows']),9)
        self.assertTrue(all(x['accepted'] is False and x['actual_player_action']=='NOT_VERIFIED' for x in report['rows']))
        self.assertFalse(report['full_parity_accepted'])

    def test_missing_renamed_duplicate_requirement_and_matrix_rows_rejected(self):
        for catalog in (self.catalog.replace('"minecraft:animals_bred",','',1),
                        self.catalog.replace('"minecraft:animals_bred"','"minecraft:clean_armor"',1)):
            with self.assertRaises(ValueError):custom_keys(self.requirements,catalog)
        with self.assertRaises(ValueError):custom_keys(self.requirements.replace('minecraft:animals_bred','minecraft:fake',1),self.catalog)
        with self.assertRaises(ValueError):inventory(self.requirements,self.catalog,self.matrix.replace('| minecraft:animals_bred |','| minecraft:fake |',1))
        with self.assertRaises(ValueError):inventory(self.requirements,self.catalog,
            self.matrix+'\n| 1 | minecraft:animals_bred | duplicate | CODE_ONLY |\n')
