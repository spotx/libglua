#pragma once

#include "glua/GluaBase.h"

#include <iostream>
#include <vector>
#include <v8.h>

namespace kdk::glua
{
class GluaV8 : public GluaBase
{
public:
    /**
     * @brief Constructs a new GluaV8 object
     *
     * @param output_stream stream to which lua 'print' output will be redirected
     * @param start_sandboxed true if the starting environment should be sandboxed
     *                        to protect from dangerous functions like file i/o, etc
     */
    explicit GluaV8(std::ostream& output_stream, bool start_sandboxed = true);

    GluaV8(const GluaV8&)     = delete;
    GluaV8(GluaV8&&) noexcept = default;

    auto operator=(const GluaV8&) -> GluaV8& = delete;
    auto operator=(GluaV8&&) noexcept -> GluaV8& = delete;

    /** GluaBase public interface, implemented by language specific derivations **/

    /**
     * @copydoc GluaBase::ResetEnvironment(bool)
     */
    auto ResetEnvironment(bool sandboxed = true) -> void override;
    /**
     * @copydoc GluaBase::RegisterCallable(const std::string&, Callable)
     */
    auto RegisterCallable(const std::string& name, Callable callable) -> void override;
    /**
     * @copydoc GluaBase::RunScript(std::string_view)
     */
    auto RunScript(std::string_view script_data) -> void override;
    /**
     * @copydoc GluaBase::RunFile(std::string_view)
     */
    auto RunFile(std::string_view file_name) -> void override;
    /*****************************************************************************/

    /**
     * @brief defaulted destructor override
     */
    ~GluaV8() override = default;

protected:
    /** GluaBase protected interface, implemented by language specific derivations **/
    auto push(std::nullopt_t /*unused*/) -> void override;
    auto push(bool value) -> void override;
    auto push(int8_t value) -> void override;
    auto push(int16_t value) -> void override;
    auto push(int32_t value) -> void override;
    auto push(int64_t value) -> void override;
    auto push(uint8_t value) -> void override;
    auto push(uint16_t value) -> void override;
    auto push(uint32_t value) -> void override;
    auto push(uint64_t value) -> void override;
    auto push(float value) -> void override;
    auto push(double value) -> void override;
    auto push(const char* value) -> void override;
    auto push(std::string_view value) -> void override;
    auto push(std::string value) -> void override;
    auto pushArray(size_t size_hint) -> void override;
    auto pushStartMap(size_t size_hint) -> void override;
    auto arraySetFromStack() -> void override;
    auto mapSetFromStack() -> void override;
    auto pushUserType(const std::string& unique_type_name, std::unique_ptr<IManagedTypeStorage> user_storage)
        -> void override;
    auto getBool(int stack_index) const -> bool override;
    auto getInt8(int stack_index) const -> int8_t override;
    auto getInt16(int stack_index) const -> int16_t override;
    auto getInt32(int stack_index) const -> int32_t override;
    auto getInt64(int stack_index) const -> int64_t override;
    auto getUInt8(int stack_index) const -> uint8_t override;
    auto getUInt16(int stack_index) const -> uint16_t override;
    auto getUInt32(int stack_index) const -> uint32_t override;
    auto getUInt64(int stack_index) const -> uint64_t override;
    auto getFloat(int stack_index) const -> float override;
    auto getDouble(int stack_index) const -> double override;
    auto getCharPointer(int stack_index) const -> const char* override;
    auto getStringView(int stack_index) const -> std::string_view override;
    auto getString(int stack_index) const -> std::string override;
    auto getArraySize(int stack_index) const -> size_t override;
    auto getArrayValue(size_t index_into_array, int stack_index_of_array) const -> void override;
    auto getMapKeys(int stack_index) const -> std::vector<std::string> override;
    auto getMapValue(const std::string& key, int stack_index_of_map) const -> void override;
    auto getUserType(const std::string& unique_type_name, int stack_index) const -> IManagedTypeStorage* override;
    auto isNull(int stack_index) const -> bool override;
    auto isBool(int stack_index) const -> bool override;
    auto isInt8(int stack_index) const -> bool override;
    auto isInt16(int stack_index) const -> bool override;
    auto isInt32(int stack_index) const -> bool override;
    auto isInt64(int stack_index) const -> bool override;
    auto isUInt8(int stack_index) const -> bool override;
    auto isUInt16(int stack_index) const -> bool override;
    auto isUInt32(int stack_index) const -> bool override;
    auto isUInt64(int stack_index) const -> bool override;
    auto isFloat(int stack_index) const -> bool override;
    auto isDouble(int stack_index) const -> bool override;
    auto isCharPointer(int stack_index) const -> bool override;
    auto isStringView(int stack_index) const -> bool override;
    auto isString(int stack_index) const -> bool override;
    auto isArray(int stack_index) const -> bool override;
    auto isMap(int stack_index) const -> bool override;
    auto setGlobalFromStack(const std::string& name, int stack_index) -> void override;
    auto pushGlobal(const std::string& name) -> void override;
    auto popOffStack(size_t count) -> void override;
    auto callScriptFunctionImpl(const std::string& function_name, size_t arg_count = 0) -> void override;
    auto registerClassImpl(
        const std::string&                                          class_name,
        std::unordered_map<std::string, std::unique_ptr<ICallable>> method_callables) -> void override;
    auto registerMethodImpl(const std::string& class_name, const std::string& method_name, Callable method)
        -> void override;
    auto transformObjectIndex(size_t index) -> size_t override;
    auto transformFunctionParameterIndex(size_t index) -> size_t override;
    /********************************************************************************/

private:
    auto absoluteIndex(int index) const -> int;
    auto getAtStackPos(int index) const -> v8::Local<v8::Value>;
    struct isolate_disposer
    {
        auto operator()(v8::Isolate* isolate) noexcept -> void { isolate->Dispose(); }
    };

    struct ScopeWrapper
    {
        ScopeWrapper(v8::Isolate* isolate) : m_scope(isolate) {}
        v8::HandleScope m_scope;
    };

    std::unique_ptr<v8::ArrayBuffer::Allocator>            m_allocator;
    std::unique_ptr<v8::Isolate, isolate_disposer>         m_isolate;
    std::unique_ptr<ScopeWrapper>                          m_scope;
    v8::Local<v8::ObjectTemplate>                          m_global_template;
    mutable std::vector<v8::Global<v8::Value>>             m_stack;
    std::unordered_map<std::string, v8::Global<v8::Value>> m_global_values;

    std::unordered_map<std::string, std::unique_ptr<ICallable>>                                  m_registry;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<ICallable>>> m_method_registry;
    std::unordered_map<std::type_index, std::string> m_class_to_metatable_name;

    std::ostream& m_output_stream;

    std::optional<size_t>      m_current_array_index;
    std::optional<std::string> m_current_map_key;

    friend auto call_callable_from_v8(const v8::FunctionCallbackInfo<v8::Value>& info) -> void;
};

auto call_callable_from_v8(const v8::FunctionCallbackInfo<v8::Value>& info) -> void;
auto destruct_managed_type(v8::Handle<v8::Context> state) -> void;

} // namespace kdk::glua
