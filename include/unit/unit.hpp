#ifndef UNIT_HPP
#define UNIT_HPP

#include <cstdint>
#include <stdexcept>

#include <unit/unit.h>

namespace unit {

inline constexpr uint32_t version_hex = UNIT_VERSION_HEX;
inline constexpr int version_major = UNIT_VERSION_MAJOR;
inline constexpr int version_minor = UNIT_VERSION_MINOR;
inline constexpr int version_patch = UNIT_VERSION_PATCH;
inline constexpr int version_dev = UNIT_VERSION_DEV;
inline constexpr const char *version_string = UNIT_VERSION_STRING;

[[nodiscard]] inline constexpr uint32_t
pack_version(int major, int minor, int patch)
{
    return UNIT_PACK_VERSION(major, minor, patch);
}

[[nodiscard]] inline constexpr uint32_t
pack_version(int major, int minor, int patch, int dev)
{
    return UNIT_PACK_VERSION_FULL(major, minor, patch, dev);
}

enum class Flag : std::uint32_t {
    NONE = UNIT_FLAG_NONE,
    NO_OPTIMIZE_TRANSLATION = UNIT_FLAG_NO_OPTIMIZE_TRANSLATION,
    FORCE_NO_INLINE = UNIT_FLAG_FORCE_NO_INLINE,
    FORCE_INLINE = UNIT_FLAG_FORCE_INLINE,
    PRINT_TRANSLATION_PREOP = UNIT_FLAG_PRINT_TRANSLATION_PREOP,
    PRINT_TRANSLATION_POSTOP = UNIT_FLAG_PRINT_TRANSLATION_POSTOP
};

inline Flag operator|(Flag a, Flag b)
{
    return static_cast<Flag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

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

    Context(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    [[nodiscard]] UNIT_Context *
    raw()
    {
        return &context;
    }
};

class ExecutableFormat {
public:
    enum Value {
        ELF = UNIT_FORMAT_ELF,
        MACHO = UNIT_FORMAT_MACHO,
        PE = UNIT_FORMAT_PE,
    };

    constexpr ExecutableFormat(Value v) : value_(v) {}

    constexpr operator Value() const { return value_; }
    explicit operator bool() const = delete;

    static ExecutableFormat host() {
#ifdef UNIT_HOST_FORMAT
        return ExecutableFormat(static_cast<Value>(UNIT_HOST_FORMAT));
#else
        throw std::runtime_error("No host format available");
#endif
    }

private:
    Value value_;
};

class SymbolMap {
    UNIT_SymbolMap symbol_map;

public:
    explicit SymbolMap(Context &ctx)
    {
        if (UNIT_FAILED(UNIT_SymbolMap_Init(&symbol_map, ctx.raw()))) {
            throw error(ctx.raw());
        }
    }

    ~SymbolMap()
    {
        UNIT_SymbolMap_Clear(&symbol_map);
    }

    SymbolMap(const SymbolMap &) = delete;
    SymbolMap(SymbolMap &&) = delete;
    SymbolMap &operator=(const SymbolMap &) = delete;
    SymbolMap &operator=(SymbolMap &&) = delete;

    void register_symbol(const std::string &name, void *address)
    {
        if (UNIT_FAILED(UNIT_SymbolMap_RegisterSymbol(&symbol_map,
                                                      name.c_str(), address))) {
            throw std::runtime_error("failed to register symbol");
        }
    }

    [[nodiscard]] UNIT_SymbolMap *
    raw()
    {
        return &symbol_map;
    }
};

template <typename Function>
class ExecutableBuffer {
    UNIT_ExecutableBuffer *buffer;

    explicit ExecutableBuffer(UNIT_ExecutableBuffer *b)
        : buffer(b) {}

public:
    ~ExecutableBuffer()
    {
        if (buffer != nullptr) {
            UNIT_ExecutableBuffer_Free(buffer);
        }
    }

    ExecutableBuffer(const ExecutableBuffer &) = delete;
    ExecutableBuffer &operator=(const ExecutableBuffer &) = delete;
    ExecutableBuffer &operator=(ExecutableBuffer &&) = delete;

    ExecutableBuffer(ExecutableBuffer &&other) noexcept
        : buffer(other.buffer)
    {
        other.buffer = nullptr;
    }

    template <typename... Args>
    auto operator()(Args... args) const
    {
        return reinterpret_cast<Function>(UNIT_ExecutableBuffer_GetPointer(buffer))(args...);
    }

    friend class CompiledProcedure;
};

class CompiledProcedure {
    UNIT_CompiledProcedure *compiled;

    explicit CompiledProcedure(UNIT_CompiledProcedure *c) : compiled(c) {}
public:

    ~CompiledProcedure()
    {
        if (compiled != nullptr) {
            UNIT_CompiledProcedure_Free(compiled);
        }
    }

    CompiledProcedure(const CompiledProcedure &) = delete;
    CompiledProcedure &operator=(const CompiledProcedure &) = delete;
    CompiledProcedure &operator=(CompiledProcedure &&) = delete;

    CompiledProcedure(CompiledProcedure &&other) noexcept
        : compiled(other.compiled)
    {
        other.compiled = nullptr;
    }

    template <typename Function>
    [[nodiscard]] ExecutableBuffer<Function>
    jit()
    {
        UNIT_ExecutableBuffer *buffer = UNIT_CompiledProcedure_JIT(compiled, nullptr);
        if (buffer == NULL) {
            throw error(compiled->context);
        }
        return ExecutableBuffer<Function>(buffer);
    }

    template <typename Function>
    [[nodiscard]] ExecutableBuffer<Function>
    jit(SymbolMap &symbol_map)
    {
        UNIT_ExecutableBuffer *buffer = UNIT_CompiledProcedure_JIT(compiled, symbol_map.raw());
        if (buffer == NULL) {
            throw error(compiled->context);
        }
        return ExecutableBuffer<Function>(buffer);
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
                                                               static_cast<UNIT_ExecutableFormat>(static_cast<int>(format))))) {
            throw error(compiled->context);
        }
    }

    void
    print_translated(FILE *stream = stdout)
    {
        UNIT_CompiledProcedure_PrintTranslatedIR(compiled, stream);
    }

    friend class Procedure;
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

class Procedure;

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

class JumpLabel {
    UNIT_JumpLabel *label;
public:
    explicit JumpLabel(UNIT_JumpLabel *l) : label(l) {}

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

    Procedure(const Procedure &) = delete;
    Procedure(Procedure &&) = delete;
    Procedure &operator=(const Procedure &) = delete;
    Procedure &operator=(Procedure &&) = delete;

    [[nodiscard]] UNIT_Procedure *
    raw()
    {
        return &procedure;
    }

    [[nodiscard]] Local
    create_local(const std::string &name)
    {
        UNIT_Local local;
        if (UNIT_FAILED(UNIT_Procedure_CreateLocal(&procedure, name.c_str(), &local))) {
            throw error(procedure.context);
        }

        return Local(local);
    }

    void
    store_name(Local local)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddStoreName(&procedure, local.raw()))) {
            throw error(procedure.context);
        }
    }

    void
    load_name(Local local)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddLoadName(&procedure, local.raw()))) {
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
        if (UNIT_FAILED(UNIT_Procedure_AddCallName(&procedure, name.c_str(), nargs))) {
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

    [[nodiscard]] JumpLabel
    create_jump_label(const std::string &name)
    {
        UNIT_JumpLabel *label = UNIT_Procedure_CreateJumpLabel(&procedure, name.c_str());
        if (label == NULL) {
            throw error(procedure.context);
        }

        return JumpLabel(label);
    }

    void
    use_label(JumpLabel label)
    {
        if (UNIT_FAILED(UNIT_Procedure_UseLabel(&procedure, label.raw()))) {
            throw error(procedure.context);
        }
    }

private:
    void
    add_op_jump(OpCode opcode, JumpLabel label)
    {
        if (UNIT_FAILED(UNIT_Procedure_AddJump(&procedure, static_cast<UNIT_Instruction>(opcode),
                                               label.raw()))) {
            throw error(procedure.context);
        }
    }

public:
    void
    jump_to(JumpLabel label)
    {
        add_op_jump(OpCode::JUMP_TO, label);
    }

    void
    jump_if_true(JumpLabel label)
    {
        add_op_jump(OpCode::JUMP_IF_TRUE, label);
    }

    void
    jump_if_false(JumpLabel label)
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

    [[nodiscard]] CompiledProcedure
    compile(Platform platform)
    {
        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, platform.raw());
        if (compiled == NULL) {
            throw error(procedure.context);
        }

        return CompiledProcedure(compiled);
    }

    void
    print_instructions(FILE *stream = stdout)
    {
        if (UNIT_FAILED(UNIT_Procedure_PrintInstructions(&procedure, stream, 1))) {
            throw error(procedure.context);
        }
    }

    void
    optimize()
    {
        if (UNIT_FAILED(UNIT_Procedure_Optimize(&procedure))) {
            throw error(procedure.context);
        }
    }

    void
    set_flags(Flag flags)
    {
        UNIT_Procedure_SetFlags(&procedure, static_cast<uint32_t>(flags));
    }

    [[nodiscard]] Flag
    get_flags() const
    {
        return static_cast<Flag>(UNIT_Procedure_GetFlags(&procedure));
    }
};

}

#endif
