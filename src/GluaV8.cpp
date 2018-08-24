#include "glua/GluaV8.h"

#include "glua/FileUtil.h"

#include <mutex>
#include <libplatform/libplatform.h>

namespace kdk::glua
{
struct V8Instance
{
    V8Instance() : m_platform(v8::platform::NewDefaultPlatform())
    {
        v8::V8::InitializePlatform(m_platform.get());
        v8::V8::Initialize();
    }

    ~V8Instance()
    {
        v8::V8::Dispose();
        v8::V8::ShutdownPlatform();
    }

    std::unique_ptr<v8::Platform> m_platform;
};

static std::mutex                  g_v8_instance_lock;
static std::unique_ptr<V8Instance> g_v8_instance;

GluaV8::GluaV8(std::ostream& output_stream, bool start_sandboxed) : m_output_stream(output_stream)
{
    (void)m_output_stream;
    {
        std::lock_guard g{g_v8_instance_lock};
        if (!g_v8_instance)
        {
            g_v8_instance = std::make_unique<V8Instance>();
        }
    }

    // do this here instead of initializer list in case we had to instantiate V8
    m_allocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());

    v8::Isolate::CreateParams params;
    params.array_buffer_allocator = m_allocator.get();

    m_isolate.reset(v8::Isolate::New(std::move(params)));
    m_isolate->SetData(0, this);

    m_scope = std::make_unique<ScopeWrapper>(m_isolate.get());

    m_global_template = v8::ObjectTemplate::New(m_isolate.get());

    ResetEnvironment(start_sandboxed);
}
auto GluaV8::ResetEnvironment(bool sandboxed) -> void
{
    (void)sandboxed;
}
auto GluaV8::RegisterCallable(const std::string& name, Callable callable) -> void
{
    auto pos = m_registry.emplace(name, std::move(callable).AcquireCallable());

    if (pos.second)
    {
        m_global_template->Set(
            v8::String::NewFromUtf8(
                m_isolate.get(), name.data(), v8::NewStringType::kNormal, static_cast<int>(name.size()))
                .ToLocalChecked(),
            v8::FunctionTemplate::New(
                m_isolate.get(), call_callable_from_v8, v8::External::New(m_isolate.get(), pos.first->second.get())));
    }
    else
    {
        throw exceptions::V8Exception("Failed to insert callable in call to RegisterCallable");
    }
}
auto GluaV8::RunScript(std::string_view script_data) -> void
{
    v8::Isolate::Scope isolate_scope(m_isolate.get());

    v8::HandleScope handle_scope(m_isolate.get());

    auto context = v8::Context::New(m_isolate.get(), nullptr, m_global_template);

    v8::Context::Scope context_scope(context);

    auto source =
        v8::String::NewFromUtf8(
            m_isolate.get(), script_data.data(), v8::NewStringType::kNormal, static_cast<int>(script_data.size()))
            .ToLocalChecked();

    auto script = v8::Script::Compile(context, source).ToLocalChecked();

    auto result = script->Run(context);

    if (!result.IsEmpty())
    {
        m_stack.emplace_back(m_isolate.get(), result.ToLocalChecked());
    }
}
auto GluaV8::RunFile(std::string_view file_name) -> void
{
    auto file_str = file_util::read_all(file_name);

    RunScript(file_str);
}
auto GluaV8::push(std::nullopt_t /*unused*/) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Object::New(m_isolate.get()));
}
auto GluaV8::push(bool value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Boolean::New(m_isolate.get(), value));
}
auto GluaV8::push(int8_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Int32::New(m_isolate.get(), value));
}
auto GluaV8::push(int16_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Int32::New(m_isolate.get(), value));
}
auto GluaV8::push(int32_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Int32::New(m_isolate.get(), value));
}
auto GluaV8::push(int64_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Int32::New(m_isolate.get(), static_cast<int32_t>(value)));
}
auto GluaV8::push(uint8_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Uint32::New(m_isolate.get(), value));
}
auto GluaV8::push(uint16_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Uint32::New(m_isolate.get(), value));
}
auto GluaV8::push(uint32_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Uint32::New(m_isolate.get(), value));
}
auto GluaV8::push(uint64_t value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Uint32::New(m_isolate.get(), static_cast<uint32_t>(value)));
}
auto GluaV8::push(float value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Number::New(m_isolate.get(), value));
}
auto GluaV8::push(double value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Number::New(m_isolate.get(), value));
}
auto GluaV8::push(const char* value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(
        m_isolate.get(), v8::String::NewFromUtf8(m_isolate.get(), value, v8::NewStringType::kNormal).ToLocalChecked());
}
auto GluaV8::push(std::string_view value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(
        m_isolate.get(),
        v8::String::NewFromUtf8(m_isolate.get(), value.data(), v8::NewStringType::kNormal, static_cast<int>(value.size()))
            .ToLocalChecked());
}
auto GluaV8::push(std::string value) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(
        m_isolate.get(),
        v8::String::NewFromUtf8(m_isolate.get(), value.data(), v8::NewStringType::kNormal, static_cast<int>(value.size()))
            .ToLocalChecked());
}
auto GluaV8::pushArray(size_t size_hint) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), v8::Array::New(m_isolate.get(), static_cast<int>(size_hint)));
}
auto GluaV8::pushStartMap(size_t size_hint) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    (void)size_hint;
    m_stack.emplace_back(m_isolate.get(), v8::Object::New(m_isolate.get()));
}
auto GluaV8::arraySetFromStack() -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};

    // top of stack: value
    // top -1: index
    // top -2: array
    auto value = m_stack.back().Get(m_isolate.get());
    m_stack.pop_back();

    auto index = m_stack.back().Get(m_isolate.get());
    m_stack.pop_back();

    auto array = v8::Local<v8::Array>::Cast(getAtStackPos(-1));

    array->Set(m_isolate->GetCurrentContext(), index, value).ToChecked();
}
auto GluaV8::mapSetFromStack() -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};

    // top of stack: value
    // top -1: key
    // top -2: map
    auto value = m_stack.back().Get(m_isolate.get());
    m_stack.pop_back();

    auto key = m_stack.back().Get(m_isolate.get());
    m_stack.pop_back();

    auto map = v8::Local<v8::Object>::Cast(getAtStackPos(-1));

    map->Set(m_isolate->GetCurrentContext(), key, value).ToChecked();
}
auto GluaV8::pushUserType(const std::string& unique_type_name, std::unique_ptr<IManagedTypeStorage> user_storage) -> void
{
    (void)unique_type_name;
    (void)user_storage;
}
auto GluaV8::getBool(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return getAtStackPos(stack_index)->ToBoolean()->Value();
}
auto GluaV8::getInt8(int stack_index) const -> int8_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return static_cast<int8_t>(
        getAtStackPos(stack_index)->ToInt32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value());
}
auto GluaV8::getInt16(int stack_index) const -> int16_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return static_cast<int16_t>(
        getAtStackPos(stack_index)->ToInt32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value());
}
auto GluaV8::getInt32(int stack_index) const -> int32_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return getAtStackPos(stack_index)->ToInt32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value();
}
auto GluaV8::getInt64(int stack_index) const -> int64_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return getAtStackPos(stack_index)->ToInt32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value();
}
auto GluaV8::getUInt8(int stack_index) const -> uint8_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return static_cast<uint8_t>(
        getAtStackPos(stack_index)->ToUint32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value());
}
auto GluaV8::getUInt16(int stack_index) const -> uint16_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return static_cast<uint16_t>(
        getAtStackPos(stack_index)->ToUint32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value());
}
auto GluaV8::getUInt32(int stack_index) const -> uint32_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return getAtStackPos(stack_index)->ToUint32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value();
}
auto GluaV8::getUInt64(int stack_index) const -> uint64_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return getAtStackPos(stack_index)->ToUint32(m_isolate->GetCurrentContext()).ToLocalChecked()->Value();
}
auto GluaV8::getFloat(int stack_index) const -> float
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return static_cast<float>(
        getAtStackPos(stack_index)->ToNumber(m_isolate->GetCurrentContext()).ToLocalChecked()->Value());
}
auto GluaV8::getDouble(int stack_index) const -> double
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return getAtStackPos(stack_index)->ToNumber(m_isolate->GetCurrentContext()).ToLocalChecked()->Value();
}
auto GluaV8::getCharPointer(int stack_index) const -> const char*
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto str = getAtStackPos(stack_index)->ToString(m_isolate->GetCurrentContext()).ToLocalChecked();

    auto external_resource = str->GetExternalOneByteStringResource();

    return external_resource->data();
}
auto GluaV8::getStringView(int stack_index) const -> std::string_view
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto str = getAtStackPos(stack_index)->ToString(m_isolate->GetCurrentContext()).ToLocalChecked();

    auto external_resource = str->GetExternalOneByteStringResource();

    return std::string_view{external_resource->data(), external_resource->length()};
}
auto GluaV8::getString(int stack_index) const -> std::string
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto str = getAtStackPos(stack_index)->ToString(m_isolate->GetCurrentContext()).ToLocalChecked();

    std::string result;
    result.resize(str->Utf8Length());

    str->WriteOneByte(reinterpret_cast<uint8_t*>(result.data()));

    return result;
}
auto GluaV8::getArraySize(int stack_index) const -> size_t
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    return v8::Local<v8::Array>::Cast(getAtStackPos(stack_index))->Length();
}
auto GluaV8::getArrayValue(size_t index_into_array, int stack_index_of_array) const -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     array = v8::Local<v8::Array>::Cast(getAtStackPos(stack_index_of_array));

    m_stack.emplace_back(
        m_isolate.get(),
        array->Get(m_isolate->GetCurrentContext(), static_cast<uint32_t>(index_into_array)).ToLocalChecked());
}
auto GluaV8::getMapKeys(int stack_index) const -> std::vector<std::string>
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     map = v8::Local<v8::Object>::Cast(getAtStackPos(stack_index));

    std::vector<std::string> keys;
    auto                     map_keys = map->GetPropertyNames(m_isolate->GetCurrentContext()).ToLocalChecked();
    // every even index is the key
    auto length = map_keys->Length();
    for (uint32_t i = 0; i < length; ++i)
    {
        auto key_value = map_keys->Get(m_isolate->GetCurrentContext(), i).ToLocalChecked();
        auto str       = key_value->ToString(m_isolate->GetCurrentContext()).ToLocalChecked();

        std::string result;
        result.resize(str->Utf8Length());

        str->WriteOneByte(reinterpret_cast<uint8_t*>(result.data()));

        keys.emplace_back(std::move(result));
    }

    return keys;
}
auto GluaV8::getMapValue(const std::string& key, int stack_index_of_map) const -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     map = v8::Local<v8::Object>::Cast(getAtStackPos(stack_index_of_map));
    auto                     str_value =
        v8::String::NewFromUtf8(m_isolate.get(), key.data(), v8::NewStringType::kNormal, static_cast<int>(key.size()))
            .ToLocalChecked();

    m_stack.emplace_back(m_isolate.get(), map->Get(m_isolate->GetCurrentContext(), str_value).ToLocalChecked());
}
auto GluaV8::getUserType(const std::string& unique_type_name, int stack_index) const -> IManagedTypeStorage*
{
    (void)unique_type_name;
    (void)stack_index;

    return nullptr;
}
auto GluaV8::isNull(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsNull();
}
auto GluaV8::isBool(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsBoolean();
}
auto GluaV8::isInt8(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsInt32();
}
auto GluaV8::isInt16(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsInt32();
}
auto GluaV8::isInt32(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsInt32();
}
auto GluaV8::isInt64(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsBigInt();
}
auto GluaV8::isUInt8(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsUint32();
}
auto GluaV8::isUInt16(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsUint32();
}
auto GluaV8::isUInt32(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsUint32();
}
auto GluaV8::isUInt64(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsBigInt();
}
auto GluaV8::isFloat(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsNumber();
}
auto GluaV8::isDouble(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsNumber();
}
auto GluaV8::isCharPointer(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsString();
}
auto GluaV8::isStringView(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsString();
}
auto GluaV8::isString(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsString();
}
auto GluaV8::isArray(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsArray();
}
auto GluaV8::isMap(int stack_index) const -> bool
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     val = getAtStackPos(stack_index);

    return val->IsObject();
}
auto GluaV8::setGlobalFromStack(const std::string& name, int stack_index) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    auto                     stack_value = getAtStackPos(stack_index);
    m_global_values.emplace(
        std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(m_isolate.get(), stack_value));
}
auto GluaV8::pushGlobal(const std::string& name) -> void
{
    v8::EscapableHandleScope scope{m_isolate.get()};
    m_stack.emplace_back(m_isolate.get(), m_global_values[name].Get(m_isolate.get()));
}
auto GluaV8::popOffStack(size_t count) -> void
{
    m_stack.resize(m_stack.size() - count);
}
auto GluaV8::callScriptFunctionImpl(const std::string& function_name, size_t arg_count) -> void
{
    (void)function_name;
    (void)arg_count;
}
auto GluaV8::registerClassImpl(
    const std::string&                                          class_name,
    std::unordered_map<std::string, std::unique_ptr<ICallable>> method_callables) -> void
{
    (void)class_name;
    (void)method_callables;
}
auto GluaV8::registerMethodImpl(const std::string& class_name, const std::string& method_name, Callable method) -> void
{
    (void)class_name;
    (void)method_name;
    (void)method;
}
auto GluaV8::transformObjectIndex(size_t index) -> size_t
{
    return index;
}
auto GluaV8::transformFunctionParameterIndex(size_t index) -> size_t
{
    return index;
}
auto GluaV8::absoluteIndex(int index) const -> int
{
    if (index < 0)
    {
        return static_cast<int>(m_stack.size()) + index;
    }

    return index;
}
auto GluaV8::getAtStackPos(int index) const -> v8::Local<v8::Value>
{
    auto absolute_index = absoluteIndex(index);

    return m_stack.at(absolute_index).Get(m_isolate.get());
}

auto call_callable_from_v8(const v8::FunctionCallbackInfo<v8::Value>& info) -> void
{
    v8::EscapableHandleScope scope{info.GetIsolate()};

    auto       data     = v8::Local<v8::External>::Cast(info.Data());
    ICallable* callable = static_cast<ICallable*>(data->Value());

    auto* isolate = info.GetIsolate();

    auto* glua_v8 = static_cast<GluaV8*>(isolate->GetData(0));

    auto count = info.Length();
    for (int i = 0; i < count; ++i)
    {
        glua_v8->m_stack.emplace_back(isolate, info[i]);
    }

    callable->Call();

    if (callable->HasReturn())
    {
        // top of stack is return value
        auto ret_val = glua_v8->m_stack.back().Get(glua_v8->m_isolate.get());
        glua_v8->m_stack.pop_back();

        info.GetReturnValue().Set(ret_val);
    }

    // pop off the stack
    glua_v8->popOffStack(count);
}
auto destruct_managed_type(v8::Handle<v8::Context> state) -> void
{
    (void)state;
}

} // namespace kdk::glua
