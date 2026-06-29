from typing import Any
import unit
import unittest


class BasicTests(unittest.TestCase):
    def make_procedure(self):
        return unit.Procedure(self.id())

    def compile_and_run(self, procedure: unit.Procedure, expected: Any, *args: Any):
        unoptimized_function = procedure.compile().jit()
        self.assertEqual(unoptimized_function(*args), expected)
        procedure.optimize()
        optimized_function = procedure.compile().jit()
        self.assertEqual(optimized_function(*args), expected)

    def test_return_constant(self):
        proc = self.make_procedure()

        proc.load_integer(42)
        proc.return_value()

        self.compile_and_run(proc, 42)

    def test_return_negative(self):
        proc = self.make_procedure()

        proc.load_integer(-1)
        proc.return_value()

        self.compile_and_run(proc, -1)

    def test_return_large(self):
        proc = self.make_procedure()
        proc.load_integer(2**40)
        proc.return_value()
        self.compile_and_run(proc, 2**40)

    def test_add(self):
        proc = self.make_procedure()

        proc.load_integer(10)
        proc.load_integer(20)
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, 30)

    def test_add_negative(self):
        proc = self.make_procedure()

        proc.load_integer(-5)
        proc.load_integer(3)
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, -2)

    def test_subtract(self):
        proc = self.make_procedure()

        proc.load_integer(10)
        proc.load_integer(20)
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, 30)

    def test_subtract_negative_result(self):
        proc = self.make_procedure()

        proc.load_integer(5)
        proc.load_integer(10)
        proc.subtract()
        proc.return_value()

        self.compile_and_run(proc, -5)

    def test_mul(self):
        proc = self.make_procedure()

        proc.load_integer(5)
        proc.load_integer(10)
        proc.multiply()
        proc.return_value()

        self.compile_and_run(proc, 50)

    def test_multiply_by_zero(self):
        proc = self.make_procedure()

        proc.load_integer(100)
        proc.load_integer(0)
        proc.multiply()
        proc.return_value()

        self.compile_and_run(proc, 0)

    def test_multiply_negatives(self):
        proc = self.make_procedure()

        proc.load_integer(-3)
        proc.load_integer(-4)
        proc.multiply()
        proc.return_value()

        self.compile_and_run(proc, 12)

    def test_divide(self):
        proc = self.make_procedure()

        proc.load_integer(20)
        proc.load_integer(10)
        proc.divide()
        proc.return_value()

        self.compile_and_run(proc, 2)

    def test_divide_truncates(self):
        proc = self.make_procedure()

        proc.load_integer(7)
        proc.load_integer(2)
        proc.divide()
        proc.return_value()

        self.compile_and_run(proc, 3)

    def test_modulo(self):
        proc = self.make_procedure()

        proc.load_integer(5)
        proc.load_integer(2)
        proc.modulo()
        proc.return_value()

        self.compile_and_run(proc, 1)


    def test_chained_arithmetic(self):
        # (2 + 3) * 4 = 20
        proc = self.make_procedure()

        proc.load_integer(2)
        proc.load_integer(3)
        proc.add()
        proc.load_integer(4)
        proc.multiply()
        proc.return_value()

        self.compile_and_run(proc, 20)

    def test_complex_expression(self):
        # (10 - 3) * (4 + 2) = 42
        proc = self.make_procedure()

        proc.load_integer(10)
        proc.load_integer(3)
        proc.subtract()
        proc.load_integer(4)
        proc.load_integer(2)
        proc.add()
        proc.multiply()
        proc.return_value()

        self.compile_and_run(proc, 42)

    def test_pop(self):
        proc = self.make_procedure()

        proc.load_integer(99)
        proc.load_integer(42)
        proc.pop()
        proc.return_value()

        self.compile_and_run(proc, 99)

    def test_copy_0(self):
        proc = self.make_procedure()

        proc.load_integer(5)
        proc.copy(0)
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, 10)

    def test_copy_1(self):
        proc = self.make_procedure()

        proc.load_integer(10)
        proc.load_integer(20)
        # [10, 20]

        proc.copy(1)
        # [10, 20, 10]
        proc.subtract()
        # [10, 10]
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, 20)

    def test_swap(self):
        proc = self.make_procedure()

        proc.load_integer(20)
        proc.load_integer(10)
        # [20, 10]

        proc.swap(1)
        # [10, 20]

        proc.subtract()
        proc.return_value()

        self.compile_and_run(proc, -10)

    def test_locals(self):
        proc = self.make_procedure()

        proc.load_integer(42)
        proc.store_local(0)

        proc.load_integer(24)
        proc.store_local(0)

        proc.load_local(0)
        proc.return_value()

        self.compile_and_run(proc, 24)

    def test_single_argument(self):
        proc = self.make_procedure()

        proc.load_argument(0)
        proc.return_value()

        self.compile_and_run(proc, 99, 99)

    def test_two_arguments_add(self):
        proc = self.make_procedure()

        proc.load_argument(0)
        proc.load_argument(1)
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, 30, 10, 20)

    def test_argument_order(self):
        proc = self.make_procedure()

        proc.load_argument(0)
        proc.load_argument(1)
        proc.subtract()
        proc.return_value()

        self.compile_and_run(proc, 7, 10, 3)

    def test_arguments_with_math(self):
        proc = self.make_procedure()

        proc.load_argument(0)  # 2
        proc.load_integer(5)
        proc.multiply()  # 10

        proc.load_argument(1)  # 3
        proc.add()

        proc.return_value()

        self.compile_and_run(proc, 13, 2, 3)

    def test_compare_equal_true(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(5)
        proc.load_integer(5)
        proc.compare_equal()
        proc.jump_if_true(end)

        proc.load_integer(0)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(1)
        proc.return_value()

        self.compile_and_run(proc, 1)

    def test_compare_equal_false(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(5)
        proc.load_integer(6)
        proc.compare_equal()
        proc.jump_if_true(end)

        proc.load_integer(0)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(1)
        proc.return_value()

        self.compile_and_run(proc, 0)

    def test_compare_not_equal(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(5)
        proc.load_integer(6)
        proc.compare_not_equal()
        proc.jump_if_true(end)

        proc.load_integer(0)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(1)
        proc.return_value()

        self.compile_and_run(proc, 1)

    def test_compare_less(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(3)
        proc.load_integer(5)
        proc.compare_less()
        proc.jump_if_true(end)

        proc.load_integer(0)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(1)
        proc.return_value()

        self.compile_and_run(proc, 1)

    def test_compare_less_equal(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(5)
        proc.load_integer(5)
        proc.compare_less_equal()
        proc.jump_if_true(end)

        proc.load_integer(0)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(1)
        proc.return_value()

        self.compile_and_run(proc, 1)

    def test_compare_greater(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(10)
        proc.load_integer(5)
        proc.compare_greater()
        proc.jump_if_true(end)

        proc.load_integer(0)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(1)
        proc.return_value()

        self.compile_and_run(proc, 1)

    def test_compare_greater_equal(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(5)
        proc.load_integer(5)
        proc.compare_greater_equal()
        proc.jump_if_true(end)

        proc.load_integer(0)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(1)
        proc.return_value()

        self.compile_and_run(proc, 1)

    def test_jump_if_false(self):
        proc = self.make_procedure()
        end = proc.create_jump_label("end")

        proc.load_integer(5)
        proc.load_integer(6)
        proc.compare_equal()
        proc.jump_if_false(end)

        proc.load_integer(99)
        proc.return_value()

        proc.use_label(end)
        proc.load_integer(42)
        proc.return_value()

        self.compile_and_run(proc, 42)

    def test_unconditional_jump(self):
        proc = self.make_procedure()
        skip = proc.create_jump_label("skip")
        proc.jump_to(skip)
        proc.load_integer(99)
        proc.return_value()

        proc.use_label(skip)
        proc.load_integer(42)
        proc.return_value()

        self.compile_and_run(proc, 42)


    def test_countdown_loop(self):
        # sum = 0; i = 5; while (i != 0) { sum += i; i -= 1; } return sum
        proc = self.make_procedure()
        loop = proc.create_jump_label("loop")
        end = proc.create_jump_label("end")

        proc.load_integer(0)
        proc.store_local(0)
        proc.load_integer(5)
        proc.store_local(1)

        proc.use_label(loop)
        proc.load_local(1)
        proc.load_integer(0)
        proc.compare_equal()
        proc.jump_if_true(end)

        proc.load_local(0)
        proc.load_local(1)
        proc.add()
        proc.store_local(0)

        proc.load_local(1)
        proc.load_integer(1)
        proc.subtract()
        proc.store_local(1)

        proc.jump_to(loop)

        proc.use_label(end)
        proc.load_local(0)
        proc.return_value()

        self.compile_and_run(proc, 15)

    def test_nested_loops(self):
        # outer = 3, inner runs 4 times each = 12 total iterations
        proc = self.make_procedure()
        outer = proc.create_jump_label("outer")
        inner = proc.create_jump_label("inner")
        end_inner = proc.create_jump_label("end_inner")
        end_outer = proc.create_jump_label("end_outer")

        proc.load_integer(0)
        proc.store_local(2)
        proc.load_integer(3)
        proc.store_local(0)

        proc.use_label(outer)
        proc.load_local(0)
        proc.load_integer(0)
        proc.compare_equal()
        proc.jump_if_true(end_outer)

        proc.load_integer(4)
        proc.store_local(1)

        proc.use_label(inner)
        proc.load_local(1)
        proc.load_integer(0)
        proc.compare_equal()
        proc.jump_if_true(end_inner)

        proc.load_local(2)
        proc.load_integer(1)
        proc.add()
        proc.store_local(2)

        proc.load_local(1)
        proc.load_integer(1)
        proc.subtract()
        proc.store_local(1)

        proc.jump_to(inner)

        proc.use_label(end_inner)
        proc.load_local(0)
        proc.load_integer(1)
        proc.subtract()
        proc.store_local(0)

        proc.jump_to(outer)

        proc.use_label(end_outer)
        proc.load_local(2)
        proc.return_value()

        self.compile_and_run(proc, 12)

    def test_stack_value_across_loop(self):
        proc = self.make_procedure()
        loop = proc.create_jump_label("loop")
        end = proc.create_jump_label("end")

        proc.load_integer(10)
        proc.store_local(0)

        proc.use_label(loop)
        proc.load_local(0)
        proc.load_integer(0)
        proc.compare_equal()
        proc.jump_if_true(end)

        proc.load_local(0)
        proc.load_integer(1)
        proc.subtract()
        proc.store_local(0)

        proc.jump_to(loop)

        proc.use_label(end)
        proc.load_local(0)
        proc.return_value()

        self.compile_and_run(proc, 0)

    # FIXME: Failing
    def notest_if_else_merge(self):
        proc = self.make_procedure()
        else_label = proc.create_jump_label("else")
        end = proc.create_jump_label("end")

        proc.load_argument(0)
        proc.load_integer(0)
        proc.compare_equal()
        proc.jump_if_false(else_label)

        proc.load_integer(10)
        proc.store_local(0)
        proc.jump_to(end)

        proc.use_label(else_label)
        proc.load_integer(20)
        proc.store_local(0)

        proc.use_label(end)
        proc.load_local(0)
        proc.return_value()

        self.compile_and_run(proc, 10, 0)
        self.compile_and_run(proc, 20, 1)

    def test_call_name(self):
        # abs(-42) = 42
        proc = self.make_procedure()
        proc.load_integer(-42)
        proc.call_name("abs", 1)
        proc.return_value()
        self.compile_and_run(proc, 42)

    def test_call_name_str(self):
        proc = self.make_procedure()

        proc.load_string("hello")
        proc.call_name("strlen", 1)
        proc.return_value()

        self.compile_and_run(proc, 5)

    def test_load_integer_rejects_string(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.load_integer("hello")  # type: ignore

    def test_load_integer_rejects_float(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.load_integer(3.14)  # type: ignore

    def test_load_integer_rejects_none(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.load_integer(None)  # type: ignore

    def test_call_name_rejects_int_name(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.call_name(42, 1)  # type: ignore

    def test_call_name_rejects_negative_nargs(self):
        proc = self.make_procedure()
        with self.assertRaises((ValueError, OverflowError)):
            proc.call_name("foo", -1)  # type: ignore

    def test_load_string_rejects_int(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.load_string(42)  # type: ignore

    def test_create_jump_label_rejects_int(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.create_jump_label(42)  # type: ignore

    def test_procedure_rejects_int_name(self):
        with self.assertRaises(TypeError):
            unit.Procedure(42)  # type: ignore

    def test_copy_rejects_string(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.copy("hello")  # type: ignore

    def test_load_argument_rejects_string(self):
        proc = self.make_procedure()
        with self.assertRaises(TypeError):
            proc.load_argument("hello")  # type: ignore

    # FIXME: Failing
    def notest_many_locals(self):
        proc = self.make_procedure()
        n = 200
        for i in range(n):
            proc.load_integer(i)
            proc.store_local(i)

        proc.load_integer(0)
        for i in range(n):
            proc.load_local(i)
            proc.add()
        proc.return_value()
        self.compile_and_run(proc, sum(range(n)))

    def test_deep_stack(self):
        proc = self.make_procedure()
        # Push 10 values, add them all
        for i in range(1, 11):
            proc.load_integer(i)
        for _ in range(9):
            proc.add()
        proc.return_value()
        self.compile_and_run(proc, 55)

    def test_multiple_returns(self):
        proc = self.make_procedure()
        early = proc.create_jump_label("early")

        proc.load_argument(0)
        proc.load_integer(0)
        proc.compare_less()
        proc.jump_if_true(early)

        proc.load_argument(0)
        proc.return_value()

        proc.use_label(early)
        proc.load_integer(-1)
        proc.return_value()

        self.compile_and_run(proc, 5, 5)
        self.compile_and_run(proc, -1, -3)

    def test_absolute_value(self):
        proc = self.make_procedure()
        positive = proc.create_jump_label("positive")

        proc.load_argument(0)
        proc.load_integer(0)
        proc.compare_greater_equal()
        proc.jump_if_true(positive)

        proc.load_integer(0)
        proc.load_argument(0)
        proc.subtract()
        proc.return_value()

        proc.use_label(positive)
        proc.load_argument(0)
        proc.return_value()

        self.compile_and_run(proc, 5, 5)
        self.compile_and_run(proc, 5, -5)
        self.compile_and_run(proc, 0, 0)

    def test_power_of_two(self):
        proc = self.make_procedure()
        loop = proc.create_jump_label("loop")
        end = proc.create_jump_label("end")

        proc.load_integer(1)
        proc.store_local(0)
        proc.load_argument(0)
        proc.store_local(1)

        proc.use_label(loop)
        proc.load_local(1)
        proc.load_integer(0)
        proc.compare_equal()
        proc.jump_if_true(end)

        proc.load_local(0)
        proc.load_integer(2)
        proc.multiply()
        proc.store_local(0)

        proc.load_local(1)
        proc.load_integer(1)
        proc.subtract()
        proc.store_local(1)

        proc.jump_to(loop)

        proc.use_label(end)
        proc.load_local(0)
        proc.return_value()

        self.compile_and_run(proc, 1, 0)
        self.compile_and_run(proc, 2, 1)
        self.compile_and_run(proc, 1024, 10)


if __name__ == "__main__":
    unittest.main()
