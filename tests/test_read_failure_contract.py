import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def contract_is_truthful(contract):
    return (
        "`ok` is false when opening or reading fails" in contract
        and "path inspection" in contract
        and "error" in contract
        and "not a failure" in contract
        and "opens and reads successfully" in contract
        and "`ok`" in contract
        and "is true" in contract
        and "ok` is false when path inspection, opening, or reading fails" not in contract
    )


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
            self.assertTrue(contract_is_truthful(contract), relative)


if __name__ == "__main__":
    unittest.main()
