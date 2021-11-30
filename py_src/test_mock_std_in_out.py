# import sys
# from io import StringIO
# from unittest.mock import patch

# Mock stdout (search for `stdout` or `StringIO`)
# ref: https://docs.python.org/3/library/unittest.mock.html#unittest.mock.patch

# Mock stdin
# ref: https://www.py4u.net/discuss/256549
# and: https://newbedev.com/python-faking-user-input-unittest-code-example
# and: https://stackoverflow.com/questions/38861101/how-can-i-test-the-standard-input-and-standard-output-in-python-script-with-a-un

# def foo():
#     print("Something")


# @patch("sys.stdout", new_callable=StringIO)
# def test(mock_stdout):
#     foo()
#     assert mock_stdout.getvalue() == "Something\n"
#     print("woot", file=sys.stderr)


# if __name__ == "__main__":
#     test()

import unittest


class TestStringMethods(unittest.TestCase):
    def test_upper(self):
        self.assertEqual("foo".upper(), "FOO")

    def test_isupper(self):
        self.assertTrue("FOO".isupper())
        self.assertFalse("Foo".isupper())

    def test_split(self):
        s = "hello world"
        self.assertEqual(s.split(), ["hello", "world"])
        # check that s.split fails when the separator is not a string
        with self.assertRaises(TypeError):
            s.split(2)  # type: ignore


if __name__ == "__main__":
    unittest.main()
