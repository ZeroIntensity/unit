#include <unit/unit.hpp>

int main(void)
{
    unit::Context context;
    unit::Procedure procedure(context, "main");

    unit::Procedure factorial(context, "factorial");
    unit::JumpLabel base_case(factorial, "base_case");
    factorial.load_argument(0);
    // [n]
    factorial.load_integer(1);
    factorial.compare_less_equal();
    // [n <= 1]
    factorial.jump_if_true(base_case);
    // []

    factorial.load_argument(0);
    factorial.copy(0);
    factorial.load_integer(1);
    factorial.subtract();
    // [n, n - 1]
    factorial.call_procedure(factorial, 1);
    // [n, factorial(n - 1)]
    factorial.multiply();
    factorial.return_value();

    factorial.use_label(base_case);
    factorial.load_integer(1);
    factorial.return_value();

    procedure.load_string("factorial 5: %ld\n");
    procedure.load_integer(5);
    procedure.call_procedure(factorial, 1);
    // ["factorial 5: %ld\n", factorial(5)]
    procedure.call_name("printf", 2);
    procedure.pop();

    procedure.load_integer(0);
    procedure.return_value();

    auto compiled = procedure.compile(unit::Platform::host());
    compiled.write_object_file("test.o", unit::ExecutableFormat::ELF);
    return 0;
}
