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
    raw()
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
    explicit CompiledProcedure(UNIT_CompiledProcedure *c) : compiled(c) {}

    ~CompiledProcedure()
    {
        UNIT_CompiledProcedure_Free(compiled);
    }

    [[nodiscard]] UNIT_CompiledProcedure *
    raw() const
    {
        return compiled;
    }

    void
    write_object_file(const std::string &path, ExecutableFormat format)
    {
        if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, path.c_str(),
                                                               static_cast<UNIT_ExecutableFormat>(format)))) {
            throw error(compiled->context);
        }
    }

    CompiledProcedure(const CompiledProcedure &) = delete;
    CompiledProcedure &operator=(const CompiledProcedure &) = delete;
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

    CONVERT = UNIT_OP_CONVERT,
};

enum class Architecture {
    AMD64 = UNIT_ARCH_AMD64,
    AARCH64 = UNIT_ARCH_AARCH64
};

enum class ABI {
    SYSTEMV = UNIT_ABI_SYSTEMV,
    APPLE = UNIT_ABI_APPLE,
    WIN64 = UNIT_ABI_WIN64
};

class Platform {
    UNIT_Platform value;
public:
    constexpr Platform(Architecture arch, ABI abi)
        : value(static_cast<uint32_t>(arch) | static_cast<uint32_t>(abi)) {}

    constexpr explicit
    Platform(UNIT_Platform v) : value(v) {}

    [[nodiscard]] constexpr UNIT_Platform
    raw() const
    {
        return value;
    }

    [[nodiscard]] constexpr Architecture
    arch() const
    {
        return static_cast<Architecture>(value & _UNIT_ARCH_MASK);
    }

    [[nodiscard]] constexpr ABI
    abi() const
    {
        return static_cast<ABI>(value & _UNIT_ABI_MASK);
    }

    constexpr bool
    operator==(Platform other) const
    {
        return value == other.value;
    }

    constexpr bool
    operator!=(Platform other) const
    {
        return value != other.value;
    }

    static Platform
    host() {
#ifdef UNIT_HOST_PLATFORM
        return Platform(UNIT_HOST_PLATFORM);
#else
        throw std::runtime_error("This system does not support UNIT");
#endif
    }
};

enum class IntegerType {
    INT8 = UNIT_TYPE_INT8,
    INT16 = UNIT_TYPE_INT16,
    INT32 = UNIT_TYPE_INT32,
    INT64 = UNIT_TYPE_INT64,
    UINT8 = UNIT_TYPE_UINT8,
    UINT16 = UNIT_TYPE_UINT16,
    UINT32 = UNIT_TYPE_UINT32,
    UINT64 = UNIT_TYPE_UINT64
};

class Local {
    UNIT_Local local;
public:
    constexpr explicit Local(UNIT_Local l) : local(l) {}

    [[nodiscard]] UNIT_Local
    raw() const
    {
        return local;
    }

    constexpr bool
    operator==(Local other) const
    {
        return local.id == other.local.id;
    }

    constexpr bool
    operator!=(Local other) const
    {
        return local.id != other.local.id;
    }
};

class Procedure;

class JumpLabel {
    UNIT_JumpLabel *label;
public:
    explicit JumpLabel(UNIT_JumpLabel *l) : label(l) {}

    explicit JumpLabel(Procedure &procedure, const std::string &name);

    [[nodiscard]] UNIT_JumpLabel *
    raw() const
    {
        return label;
    }

    bool
    operator==(JumpLabel other) const
    {
        return label->id == other.label->id;
    }

    bool
    operator!=(JumpLabel other) const
    {
        return label->id != other.label->id;
    }
};

class Procedure {
    UNIT_Procedure procedure;

    void
    add_op_int(OpCode op, std::int64_t oparg)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure,
                                                    static_cast<UNIT_Instruction>(op),
                                                    oparg))) {
            throw error(procedure.context);
        }
    }

    void
    add_op(OpCode op)
    {
        add_op_int(op, 0);
    }

public:
    explicit Procedure(Context &context, const std::string &name)
    {
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, context.raw(), name.c_str()))) {
            throw error(context.raw());
        }
    }

    ~Procedure()
    {
        UNIT_Procedure_Clear(&procedure);
    }

    [[nodiscard]] UNIT_Procedure *
    raw()
    {
        return &procedure;
    }

    Local
    store_name(const std::string &name)
    {
        UNIT_Local local;
        if (UNIT_FAILED(UNIT_Procedure_AddStoreLocal(&procedure, name.c_str(), &local))) {
            throw error(procedure.context);
        }
        return Local(local);
    }

    void
    load_name(Local local)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddLoadLocal(&procedure, local.raw()))) {
            throw error(procedure.context);
        }
    }

    void
    store_local(int64_t index)
    {
        add_op_int(OpCode::STORE_LOCAL, index);
    }

    void
    load_local(int64_t index)
    {
        add_op_int(OpCode::LOAD_LOCAL, index);
    }

    void
    load_integer(std::int64_t value)
    {
        add_op_int(OpCode::LOAD_INTEGER, value);
    }

    void
    load_string(const std::string &value)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(&procedure, value.c_str()))) {
            throw error(procedure.context);
        }
    }

    void
    add()
    {
        add_op(OpCode::ADD);
    }

    void
    subtract()
    {
        add_op(OpCode::SUBTRACT);
    }

    void
    multiply()
    {
        add_op(OpCode::MULTIPLY);
    }

    void
    divide()
    {
        add_op(OpCode::DIVIDE);
    }

    void
    modulo()
    {
        add_op(OpCode::MODULO);
    }

    void
    exit()
    {
        add_op(OpCode::EXIT);
    }

    void
    return_value()
    {
        add_op(OpCode::RETURN_VALUE);
    }

    void
    load_argument(uint8_t index)
    {
        add_op_int(OpCode::LOAD_ARGUMENT, index);
    }

    void
    call_name(const std::string &name, uint8_t nargs)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddCall(&procedure, name.c_str(), nargs))) {
            throw error(procedure.context);
        }
    }

    void
    call_procedure(Procedure& subprocedure, uint8_t nargs)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddCallProcedure(&procedure, subprocedure.raw(), nargs))) {
            throw error(procedure.context);
        }
    }

    void
    compare_equal()
    {
        add_op(OpCode::COMPARE_EQUAL);
    }

    void
    compare_not_equal()
    {
        add_op(OpCode::COMPARE_NOT_EQUAL);
    }

    void
    compare_greater()
    {
        add_op(OpCode::COMPARE_GREATER);
    }

    void
    compare_greater_equal()
    {
        add_op(OpCode::COMPARE_GREATER_EQUAL);
    }

    void
    compare_less()
    {
        add_op(OpCode::COMPARE_LESS);
    }

    void
    compare_less_equal()
    {
        add_op(OpCode::COMPARE_LESS_EQUAL);
    }

    void
    use_label(const JumpLabel label)
    {
        if (UNIT_FAILED(UNIT_Procedure_UseLabel(&procedure, label.raw()))) {
            throw error(procedure.context);
        }
    }

private:
    void
    add_op_jump(OpCode opcode, const JumpLabel label)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddJump(&procedure, static_cast<UNIT_Instruction>(opcode),
                                               label.raw()))) {
            throw error(procedure.context);
        }
    }

public:
    void
    jump_to(const JumpLabel label)
    {
        add_op_jump(OpCode::JUMP_TO, label);
    }

    void
    jump_if_true(const JumpLabel label)
    {
        add_op_jump(OpCode::JUMP_IF_TRUE, label);
    }

    void
    jump_if_false(const JumpLabel label)
    {
        add_op_jump(OpCode::JUMP_IF_FALSE, label);
    }

    void
    copy(int64_t offset)
    {
        add_op_int(OpCode::COPY, offset);
    }

    void
    swap(int64_t offset)
    {
        add_op_int(OpCode::SWAP, offset);
    }

    void
    pop()
    {
        add_op(OpCode::POP);
    }

    void
    read_bytes(uint8_t num_bytes)
    {
        add_op_int(OpCode::READ_BYTES, num_bytes);
    }

    void
    write_bytes(uint8_t num_bytes)
    {
        add_op_int(OpCode::WRITE_BYTES, num_bytes);
    }

    void
    address_of(const Local local)
    {
        add_op_int(OpCode::ADDRESS_OF, local.raw().id);
    }

    void
    convert(IntegerType type)
    {
        add_op_int(OpCode::CONVERT, static_cast<int64_t>(type));
    }

    CompiledProcedure
    compile(Platform platform)
    {
        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, platform.raw());
        if (compiled == NULL) {
            throw error(procedure.context);
        }

        return CompiledProcedure(compiled);
    }

    Procedure(const Procedure &) = delete;
    Procedure &operator=(const Procedure &) = delete;
};

// This needs to be defined down here because we need access to the full procedure class
inline
JumpLabel::JumpLabel(Procedure &procedure, const std::string &name)
{
    auto label = UNIT_Procedure_CreateJumpLabel(procedure.raw(), name.c_str());
    if (label == NULL) {
        throw error(procedure.raw()->context);
    }
    this->label = label;
}

}

#endif
