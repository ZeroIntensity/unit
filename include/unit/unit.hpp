#ifndef UNIT_HPP
#define UNIT_HPP

#include <cstdint>
#include <stdexcept>

#include <unit/unit.h>

namespace unit {

enum class ErrorCode {
    NONE = UNIT_ERROR_NONE,
    NO_MEMORY = UNIT_ERROR_NO_MEMORY,
    INVALID_USAGE = UNIT_ERROR_INVALID_USAGE,
    OS_FAILURE = UNIT_ERROR_OS_FAILURE,
    UNSUPPORTED_PLATFORM = UNIT_ERROR_UNSUPPORTED_PLATFORM
};

class error : public std::runtime_error {
    UNIT_ErrorCode error_code;
public:
    error(UNIT_Context *ctx)
        : std::runtime_error(UNIT_GetErrorMessage(ctx))
        , error_code(UNIT_GetErrorCode(ctx))
    {}

    [[nodiscard]] ErrorCode
    code() const
    {
        return static_cast<ErrorCode>(error_code);
    }
};

class Context {
    UNIT_Context context;
public:
    Context() {
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            throw std::runtime_error("failed to initialize context");
        }
    }

    ~Context()
    {
        UNIT_Context_Clear(&context);
    }

    [[nodiscard]] UNIT_Context *
    get()
    {
        return &context;
    }

    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
};

enum class ExecutableFormat {
    ELF = UNIT_FORMAT_ELF,
    MACHO = UNIT_FORMAT_MACHO,
    PE = UNIT_FORMAT_PE,
};

class CompiledProcedure {
    UNIT_CompiledProcedure *compiled;

public:
    CompiledProcedure(UNIT_CompiledProcedure *c) : compiled(c) {}
    ~CompiledProcedure()
    {
        UNIT_CompiledProcedure_Free(compiled);
    }

    [[nodiscard]] UNIT_CompiledProcedure *
    get() const
    {
        return compiled;
    }

    void
    write_object_file(std::string path, ExecutableFormat format)
    {
        if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, path.c_str(),
                                                               static_cast<UNIT_ExecutableFormat>(format)))) {
            throw error(compiled->context);
        }
    }
};


enum class OpCode
{
    LOAD_STRING = UNIT_OP_LOAD_STRING,
    LOAD_INTEGER = UNIT_OP_LOAD_INTEGER,

    LOAD_LOCAL = UNIT_OP_LOAD_LOCAL,
    STORE_LOCAL = UNIT_OP_STORE_LOCAL,

    ADD = UNIT_OP_ADD,
    SUBTRACT = UNIT_OP_SUBTRACT,
    MULTIPLY = UNIT_OP_MULTIPLY,
    DIVIDE = UNIT_OP_DIVIDE,
    MODULO = UNIT_OP_MODULO,

    JUMP_TO = UNIT_OP_JUMP_TO,
    JUMP_IF_FALSE = UNIT_OP_JUMP_IF_FALSE,
    JUMP_IF_TRUE = UNIT_OP_JUMP_IF_TRUE,

    EXIT = UNIT_OP_EXIT,
    RETURN_VALUE = UNIT_OP_RETURN_VALUE,
    LOAD_ARGUMENT = UNIT_OP_LOAD_ARGUMENT,

    PREPARE_CALL = UNIT_OP_PREPARE_CALL,
    CALL_NAME = UNIT_OP_CALL_NAME,
    CALL_PROCEDURE = UNIT_OP_CALL_PROCEDURE,

    COMPARE_EQUAL = UNIT_OP_COMPARE_EQUAL,
    COMPARE_NOT_EQUAL = UNIT_OP_COMPARE_NOT_EQUAL,
    COMPARE_GREATER = UNIT_OP_COMPARE_GREATER,
    COMPARE_GREATER_EQUAL = UNIT_OP_COMPARE_GREATER_EQUAL,
    COMPARE_LESS = UNIT_OP_COMPARE_LESS,
    COMPARE_LESS_EQUAL = UNIT_OP_COMPARE_LESS_EQUAL,

    COPY = UNIT_OP_COPY,
    SWAP = UNIT_OP_SWAP,
    POP = UNIT_OP_POP,

    ADDRESS_OF = UNIT_OP_ADDRESS_OF,
    READ_BYTES = UNIT_OP_READ_BYTES,
    WRITE_BYTES = UNIT_OP_WRITE_BYTES,

    CAST = UNIT_OP_CAST,
    CONVERT = UNIT_OP_CONVERT,
};

using Platform = std::uint32_t;

class Procedure {
    UNIT_Procedure procedure;

    void
    add_op_int(OpCode op, std::int32_t oparg)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure,
                                                    static_cast<UNIT_Instruction>(op),
                                                    oparg))) {
            throw error(procedure.context);
        }
    }

public:
    Procedure(Context &context, const char *name)
    {
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, context.get(), name))) {
            throw error(context.get());
        }
    }

    ~Procedure()
    {
        UNIT_Procedure_Clear(&procedure);
    }

    [[nodiscard]] UNIT_Procedure *
    get()
    {
        return &procedure;
    }

    CompiledProcedure
    compile(Platform platform)
    {
        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, platform);
        if (compiled == NULL) {
            throw error(procedure.context);
        }

        return CompiledProcedure(compiled);
    }
};

}

#endif
