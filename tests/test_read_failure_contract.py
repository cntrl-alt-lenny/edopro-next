import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ReadFailureContractTests(unittest.TestCase):
    def test_load_contracts_enumerate_failure_stages_without_a_false_biconditional(self):
        for relative in (
            "data/include/edopro_next/data/ydk.h",
            "policy/include/edopro_next/policy/lf_list.h",
        ):
            text = (ROOT / relative).read_text()
            start = text.index("// The result of load_")
            end = text.index("struct ", start)
            contract = text[start:end]
            self.assertNotIn("ok` is false exactly when", contract)
            self.assertIn("path inspection", contract)
            self.assertIn("opening", contract)
            self.assertIn("reading", contract)


if __name__ == "__main__":
    unittest.main()
