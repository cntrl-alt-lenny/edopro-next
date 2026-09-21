import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]

POSITIVE_PREDICATE = "`ok` is false when opening or reading fails."
STATUS_ERROR_EXCEPTION = (
    "A path inspection error alone is not a failure: if the path then opens and reads successfully, `ok` is true."
)
CONTRADICTIONS = (
    "`ok` is false when path inspection, opening, or reading fails",
    "`ok` is true when opening or reading fails",
    "`ok` is false when opening succeeds",
    "`ok` is false when reading succeeds",
    "A path inspection error alone is a failure",
)


def contract_is_truthful(contract):
    normalized = re.sub(r"\s+", " ", re.sub(r"//\s*", " ", contract))
    return (
        POSITIVE_PREDICATE in normalized
        and STATUS_ERROR_EXCEPTION in normalized
        and not any(contradiction in normalized for contradiction in CONTRADICTIONS)
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

    def test_rejects_contradictory_positive_predicate(self):
        contract = POSITIVE_PREDICATE + "\n" + STATUS_ERROR_EXCEPTION
        contradictory = contract + "\n`ok` is true when opening or reading fails"
        self.assertFalse(contract_is_truthful(contradictory))

    def test_rejects_contradictory_status_error_exception(self):
        contract = POSITIVE_PREDICATE + "\n" + STATUS_ERROR_EXCEPTION
        contradictory = contract + "\nA path inspection error alone is a failure"
        self.assertFalse(contract_is_truthful(contradictory))


if __name__ == "__main__":
    unittest.main()
