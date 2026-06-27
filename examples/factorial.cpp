#include <iostream>
#include <string>

#include <unit/unit.hpp>

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Expected a number\n";
        return 1;
    }

    char *number_str = argv[1];
    int number;
    try {
        number = std::stoi(number_str);
    } catch (std::invalid_argument &e) {
        std::cerr << "Failed to parse number\n";
        return 1;
    } catch (std::out_of_range &e) {
        std::cerr << "Number is too big\n";
        return 1;
    }

    unit::Context context;
    unit::Procedure procedure(context, "main");

    unit::Procedure factorial(context, "factorial");
    auto base_case = factorial.create_jump_label("base_case");
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

    procedure.load_string("%ld\n");
    procedure.load_integer(number);
    procedure.call_procedure(factorial, 1);
    // ["factorial 5: %ld\n", factorial(5)]
    procedure.call_name("printf", 2);
    procedure.pop();

    procedure.load_integer(0);
    procedure.return_value();

    factorial.optimize();
    procedure.optimize();

    factorial.print_instructions();
    procedure.print_instructions();

    auto compiled = procedure.compile(unit::Platform::host());
    compiled.print_translated();
    compiled.write_object_file("test.o", unit::ExecutableFormat::host());
    return 0;
}
