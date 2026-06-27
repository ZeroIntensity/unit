#include <iostream>

#include <unit/unit.hpp>


int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Expected a number\n";
        return 1;
    }

    char *number_str = argv[1];
    long long number;
    try {
        number = std::stoll(number_str);
    } catch (std::invalid_argument &e) {
        std::cerr << "Failed to parse number\n";
        return 1;
    } catch (std::out_of_range &e) {
        std::cerr << "Number is too big\n";
        return 1;
    }

    unit::Context context;
    unit::Procedure procedure(context, "main");
    auto loop = procedure.create_jump_label("loop");
    auto is_even = procedure.create_jump_label("is_even");
    auto finished = procedure.create_jump_label("finished");

    procedure.load_integer(number);

    procedure.use_label(loop);
    procedure.copy(0);
    // [number, number]

    procedure.load_string("%d\n");
    procedure.copy(2);
    // [number, number, "%d\n", number]
    procedure.call_name("printf", 2);
    procedure.pop();

    // [number, number]
    procedure.copy(0);
    procedure.load_integer(1);
    // [number, number, number, 1]
    procedure.compare_equal();
    procedure.jump_if_true(finished);

    // [number, number]
    procedure.load_integer(2);
    procedure.modulo();
    // [number, number % 2]

    procedure.load_integer(0);
    procedure.compare_equal();
    // [number, number % 2 == 0]
    procedure.jump_if_true(is_even);

    // [number]
    procedure.load_integer(3);
    procedure.multiply();
    // [number * 3]
    procedure.load_integer(1);
    procedure.add();
    // [(number * 3) + 1]

    procedure.jump_to(loop);

    procedure.use_label(is_even);
    // [number]
    procedure.load_integer(2);
    procedure.divide();
    // [number / 2]
    procedure.jump_to(loop);

    procedure.use_label(finished);
    procedure.pop();
    procedure.load_integer(0);
    procedure.return_value();

    procedure.print_instructions();
    procedure.set_flags(unit::Flag::PRINT_TRANSLATION_PREOP);
    auto compiled = procedure.compile(unit::Platform::host());
    compiled.write_object_file("test.o", unit::ExecutableFormat::ELF);

    return 0;
}
