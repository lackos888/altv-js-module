#include "Serialization.h"
#include "V8ResourceImpl.h"
#include "Bindings.h"
#include "CProfiler.h"

alt::MValue V8Helpers::V8ToMValue(v8::Local<v8::Value> val, bool allowFunction)
{
    CProfiler::Sample _("V8Helpers::V8ToMValue", true);
    alt::ICore& core = alt::ICore::Instance();

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> ctx = isolate->GetEnteredOrMicrotaskContext();

    if(val.IsEmpty()) return core.CreateMValueNone();

    if(val->IsUndefined()) return core.CreateMValueNone();

    if(val->IsNull()) return core.CreateMValueNil();

    if(val->IsBoolean()) return core.CreateMValueBool(val->BooleanValue(isolate));

    if(val->IsInt32()) return core.CreateMValueInt(val->Int32Value(ctx).ToChecked());

    if(val->IsUint32()) return core.CreateMValueUInt(val->Uint32Value(ctx).ToChecked());

    if(val->IsBigInt())
    {
        bool isPositive;
        uint64_t unsignedVal = val.As<v8::BigInt>()->Uint64Value(&isPositive);
        if(isPositive) return core.CreateMValueUInt(unsignedVal);
        else
            return core.CreateMValueInt(val.As<v8::BigInt>()->Int64Value());
    }

    if(val->IsNumber()) return core.CreateMValueDouble(val->NumberValue(ctx).ToChecked());

    if(val->IsString()) return core.CreateMValueString(*v8::String::Utf8Value(isolate, val));

    if(val->IsObject())
    {
        if(val->IsArray())
        {
            v8::Local<v8::Array> v8Arr = val.As<v8::Array>();
            alt::MValueList list = core.CreateMValueList(v8Arr->Length());

            for(uint32_t i = 0; i < v8Arr->Length(); ++i)
            {
                v8::Local<v8::Value> value;
                if(!v8Arr->Get(ctx, i).ToLocal(&value)) continue;
                list->Set(i, V8ToMValue(value, allowFunction));
            }

            return list;
        }
        else if(val->IsFunction())
        {
            if(!allowFunction)
            {
                Log::Error << V8Helpers::SourceLocation::GetCurrent(isolate).ToString() << " "
                           << "Cannot convert function to MValue" << Log::Endl;
                return core.CreateMValueNone();
            }
            v8::Local<v8::Function> v8Func = val.As<v8::Function>();
            return V8ResourceImpl::Get(ctx)->GetFunction(v8Func);
        }
        else if(val->IsArrayBuffer())
        {
            auto v8Buffer = val.As<v8::ArrayBuffer>()->GetBackingStore();
            return core.CreateMValueByteArray((uint8_t*)v8Buffer->Data(), v8Buffer->ByteLength());
        }
        else if(val->IsTypedArray())
        {
            v8::Local<v8::TypedArray> typedArray = val.As<v8::TypedArray>();
            if(!typedArray->HasBuffer()) return core.CreateMValueNone();
            v8::Local<v8::ArrayBuffer> v8Buffer = typedArray->Buffer();
            return core.CreateMValueByteArray((uint8_t*)((uintptr_t)v8Buffer->GetBackingStore()->Data() + typedArray->ByteOffset()), typedArray->ByteLength());
        }
        else if(val->IsMap())
        {
            v8::Local<v8::Map> map = val.As<v8::Map>();
            v8::Local<v8::Array> mapArr = map->AsArray();
            uint32_t size = mapArr->Length();

            alt::MValueDict dict = alt::ICore::Instance().CreateMValueDict();
            for(uint32_t i = 0; i < size; i += 2)
            {
                auto maybeKey = mapArr->Get(ctx, i);
                auto maybeValue = mapArr->Get(ctx, i + 1);
                v8::Local<v8::Value> key;
                v8::Local<v8::Value> value;

                if(!maybeKey.ToLocal(&key)) continue;
                if(!maybeValue.ToLocal(&value)) continue;
                std::string keyString = V8Helpers::Stringify(ctx, val);
                if(keyString.empty()) continue;
                dict->Set(keyString, V8ToMValue(value, false));
            }
            return dict;
        }
        else
        {
            V8ResourceImpl* resource = V8ResourceImpl::Get(ctx);
            v8::Local<v8::Object> v8Obj = val.As<v8::Object>();

            // if (v8Obj->InstanceOf(ctx, v8Vector3->JSValue(isolate, ctx)).ToChecked())
            if(resource->IsVector3(v8Obj))
            {
                v8::Local<v8::Value> x, y, z;
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::Vector3_XKey(isolate)).ToLocal(&x), "Failed to convert Vector3 to MValue", core.CreateMValueNil());
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::Vector3_YKey(isolate)).ToLocal(&y), "Failed to convert Vector3 to MValue", core.CreateMValueNil());
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::Vector3_ZKey(isolate)).ToLocal(&z), "Failed to convert Vector3 to MValue", core.CreateMValueNil());

                return core.CreateMValueVector3(alt::Vector3f{ x.As<v8::Number>()->Value(), y.As<v8::Number>()->Value(), z.As<v8::Number>()->Value() });
            }
            else if(resource->IsVector2(v8Obj))
            {
                v8::Local<v8::Value> x, y;
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::Vector3_XKey(isolate)).ToLocal(&x), "Failed to convert Vector2 to MValue", core.CreateMValueNil());
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::Vector3_YKey(isolate)).ToLocal(&y), "Failed to convert Vector2 to MValue", core.CreateMValueNil());

                return core.CreateMValueVector2(alt::Vector2f{ x.As<v8::Number>()->Value(), y.As<v8::Number>()->Value() });
            }
            else if(resource->IsRGBA(v8Obj))
            {
                v8::Local<v8::Value> r, g, b, a;
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::RGBA_RKey(isolate)).ToLocal(&r), "Failed to convert RGBA to MValue", core.CreateMValueNil());
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::RGBA_GKey(isolate)).ToLocal(&g), "Failed to convert RGBA to MValue", core.CreateMValueNil());
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::RGBA_BKey(isolate)).ToLocal(&b), "Failed to convert RGBA to MValue", core.CreateMValueNil());
                V8_CHECK_RETN(v8Obj->Get(ctx, V8Helpers::RGBA_AKey(isolate)).ToLocal(&a), "Failed to convert RGBA to MValue", core.CreateMValueNil());

                return core.CreateMValueRGBA(
                  alt::RGBA{ (uint8_t)r.As<v8::Number>()->Value(), (uint8_t)g.As<v8::Number>()->Value(), (uint8_t)b.As<v8::Number>()->Value(), (uint8_t)a.As<v8::Number>()->Value() });
            }
            else if(resource->IsBaseObject(v8Obj))
            {
                V8Entity* ent = V8Entity::Get(v8Obj);

                V8_CHECK_RETN(ent, "Unable to convert base object to MValue because it was destroyed and is now invalid", core.CreateMValueNil());
                return core.CreateMValueBaseObject(ent->GetHandle());
            }
            else
            {
                alt::MValueDict dict = core.CreateMValueDict();
                v8::Local<v8::Array> keys;

                V8_CHECK_RETN(v8Obj->GetOwnPropertyNames(ctx).ToLocal(&keys), "Failed to convert object to MValue", core.CreateMValueNil());
                for(uint32_t i = 0; i < keys->Length(); ++i)
                {
                    v8::Local<v8::Value> v8Key;
                    V8_CHECK_RETN(keys->Get(ctx, i).ToLocal(&v8Key), "Failed to convert object to MValue", core.CreateMValueNil());
                    v8::Local<v8::Value> value;
                    V8_CHECK_RETN(v8Obj->Get(ctx, v8Key).ToLocal(&value), "Failed to convert object to MValue", core.CreateMValueNil());

                    if(value->IsUndefined()) continue;
                    std::string key = *v8::String::Utf8Value(isolate, v8Key);
                    dict->Set(key, V8ToMValue(value, allowFunction));
                }

                return dict;
            }
        }
    }

    return core.CreateMValueNone();
}

v8::Local<v8::Value> V8Helpers::MValueToV8(alt::MValueConst val)
{
    CProfiler::Sample _("V8Helpers::MValueToV8", true);
    static constexpr int64_t JS_MAX_SAFE_INTEGER = 9007199254740991;
    static constexpr int64_t JS_MIN_SAFE_INTEGER = JS_MAX_SAFE_INTEGER * -1;

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> ctx = isolate->GetEnteredOrMicrotaskContext();

    switch(val->GetType())
    {
        case alt::IMValue::Type::NONE: return v8::Undefined(isolate);
        case alt::IMValue::Type::NIL: return V8Helpers::JSValue(nullptr);
        case alt::IMValue::Type::BOOL: return V8Helpers::JSValue(std::dynamic_pointer_cast<const alt::IMValueBool>(val)->Value());
        case alt::IMValue::Type::INT:
        {
            int64_t _val = std::dynamic_pointer_cast<const alt::IMValueInt>(val)->Value();

            if(_val >= JS_MIN_SAFE_INTEGER && _val <= JS_MAX_SAFE_INTEGER) return V8Helpers::JSValue((double)_val);

            return V8Helpers::JSValue(_val);
        }
        case alt::IMValue::Type::UINT:
        {
            uint64_t _val = std::dynamic_pointer_cast<const alt::IMValueUInt>(val)->Value();

            if(_val <= JS_MAX_SAFE_INTEGER) return V8Helpers::JSValue((double)_val);

            return V8Helpers::JSValue(_val);
        }
        case alt::IMValue::Type::DOUBLE: return V8Helpers::JSValue(std::dynamic_pointer_cast<const alt::IMValueDouble>(val)->Value());
        case alt::IMValue::Type::STRING: return V8Helpers::JSValue(std::dynamic_pointer_cast<const alt::IMValueString>(val)->Value());
        case alt::IMValue::Type::LIST:
        {
            alt::MValueListConst list = std::dynamic_pointer_cast<const alt::IMValueList>(val);
            v8::Local<v8::Array> v8Arr = v8::Array::New(isolate, (int)list->GetSize());

            for(uint32_t i = 0; i < list->GetSize(); ++i) v8Arr->Set(ctx, i, MValueToV8(list->Get(i)));

            return v8Arr;
        }
        case alt::IMValue::Type::DICT:
        {
            alt::MValueDictConst dict = std::dynamic_pointer_cast<const alt::IMValueDict>(val);
            v8::Local<v8::Object> v8Obj = v8::Object::New(isolate);

            for(auto it = dict->Begin(); it != dict->End(); ++it)
            {
                v8Obj->Set(ctx, V8Helpers::JSValue(it->first), MValueToV8(it->second));
            }

            return v8Obj;
        }
        case alt::IMValue::Type::BASE_OBJECT:
        {
            alt::IBaseObject* ref = std::dynamic_pointer_cast<const alt::IMValueBaseObject>(val)->RawValue();
            return V8ResourceImpl::Get(ctx)->GetBaseObjectOrNull(ref);
        }
        case alt::IMValue::Type::FUNCTION:
        {
            alt::MValueFunctionConst fn = std::dynamic_pointer_cast<const alt::IMValueFunction>(val);
            v8::Local<v8::External> extFn = v8::External::New(isolate, new alt::MValueFunctionConst(fn));

            v8::Local<v8::Function> func;
            V8_CHECK_RETN(v8::Function::New(ctx, V8Helpers::FunctionCallback, extFn).ToLocal(&func), "Failed to convert MValue to function", v8::Undefined(isolate));
            return func;
        }
        case alt::IMValue::Type::VECTOR3: return V8ResourceImpl::Get(ctx)->CreateVector3(std::dynamic_pointer_cast<const alt::IMValueVector3>(val)->Value());
        case alt::IMValue::Type::VECTOR2: return V8ResourceImpl::Get(ctx)->CreateVector2(std::dynamic_pointer_cast<const alt::IMValueVector2>(val)->Value());
        case alt::IMValue::Type::RGBA: return V8ResourceImpl::Get(ctx)->CreateRGBA(std::dynamic_pointer_cast<const alt::IMValueRGBA>(val)->Value());
        case alt::IMValue::Type::BYTE_ARRAY:
        {
            alt::MValueByteArrayConst buffer = std::dynamic_pointer_cast<const alt::IMValueByteArray>(val);
            // Check if the buffer is a raw JS value buffer
            v8::MaybeLocal<v8::Value> jsVal = RawBytesToV8(buffer);
            if(!jsVal.IsEmpty()) return jsVal.ToLocalChecked();

            v8::Local<v8::ArrayBuffer> v8Buffer = v8::ArrayBuffer::New(isolate, buffer->GetSize());
            std::memcpy(v8Buffer->GetBackingStore()->Data(), buffer->GetData(), buffer->GetSize());
            return v8Buffer;
        }
        default: Log::Warning << "V8Helpers::MValueToV8 Unknown MValue type " << (int)val->GetType() << Log::Endl;
    }

    return v8::Undefined(isolate);
}

void V8Helpers::MValueArgsToV8(alt::MValueArgs args, std::vector<v8::Local<v8::Value>>& v8Args)
{
    for(uint64_t i = 0; i < args.size(); ++i) v8Args.push_back(MValueToV8(args[i]));
}

// Magic bytes to identify raw JS value buffers
static uint8_t magicBytes[] = { 'J', 'S', 'V', 'a', 'l' };

enum class RawValueType : uint8_t
{
    INVALID,
    GENERIC,
    BASEOBJECT,
    VECTOR3,
    VECTOR2,
    RGBA
};

extern V8Class v8BaseObject;
static inline RawValueType GetValueType(v8::Local<v8::Context> ctx, v8::Local<v8::Value> val)
{
    V8ResourceImpl* resource = V8ResourceImpl::Get(ctx);
    bool result;
    if(val->IsSharedArrayBuffer() || val->IsPromise() || val->IsProxy()) return RawValueType::INVALID;
    if(val->InstanceOf(ctx, v8BaseObject.JSValue(ctx->GetIsolate(), ctx)).To(&result) && result)
    {
        V8Entity* entity = V8Entity::Get(val);
        if(!entity) return RawValueType::INVALID;
        alt::IBaseObject* object = entity->GetHandle();
        if(!object) return RawValueType::INVALID;
        return RawValueType::BASEOBJECT;
    }
    if(resource->IsVector3(val)) return RawValueType::VECTOR3;
    if(resource->IsVector2(val)) return RawValueType::VECTOR2;
    if(resource->IsRGBA(val)) return RawValueType::RGBA;
    else
        return RawValueType::GENERIC;
}

static inline bool WriteRawValue(v8::Local<v8::Context> ctx, v8::ValueSerializer& serializer, RawValueType type, v8::Local<v8::Object> val)
{
    CProfiler::Sample _("WriteRawValue", true);
    serializer.WriteRawBytes(&type, sizeof(uint8_t));
    switch(type)
    {
        case RawValueType::BASEOBJECT:
        {
            V8Entity* entity = V8Entity::Get(val);
            if(!entity) return false;
            alt::IBaseObject* handle = entity->GetHandle();
            uint8_t type = static_cast<uint8_t>(handle->GetType());

            uint32_t id;
            bool remote;
#ifdef ALT_CLIENT_API
            if (handle->IsRemote())
            {
                id = handle->GetRemoteID();
                remote = true;
            }
            else
            {
                id = handle->GetID();
                remote = false;
            }
#else
            id = handle->GetID();
            remote = true;
            
#endif // ALT_CLIENT_API

            serializer.WriteRawBytes(&id, sizeof(id));
            serializer.WriteRawBytes(&type, sizeof(type));
            serializer.WriteRawBytes(&remote, sizeof(remote));
            break;
        }
        case RawValueType::VECTOR3:
        {
            alt::Vector3f vec;
            if(!V8Helpers::SafeToVector3(val, ctx, vec)) return false;
            float x = vec[0];
            float y = vec[1];
            float z = vec[2];
            serializer.WriteRawBytes(&x, sizeof(float));
            serializer.WriteRawBytes(&y, sizeof(float));
            serializer.WriteRawBytes(&z, sizeof(float));
            break;
        }
        case RawValueType::VECTOR2:
        {
            alt::Vector2f vec;
            if(!V8Helpers::SafeToVector2(val, ctx, vec)) return false;
            float x = vec[0];
            float y = vec[1];
            serializer.WriteRawBytes(&x, sizeof(float));
            serializer.WriteRawBytes(&y, sizeof(float));
            break;
        }
        case RawValueType::RGBA:
        {
            alt::RGBA rgba;
            if(!V8Helpers::SafeToRGBA(val, ctx, rgba)) return false;
            serializer.WriteRawBytes(&rgba.r, sizeof(uint8_t));
            serializer.WriteRawBytes(&rgba.g, sizeof(uint8_t));
            serializer.WriteRawBytes(&rgba.b, sizeof(uint8_t));
            serializer.WriteRawBytes(&rgba.a, sizeof(uint8_t));
            break;
        }
    }
    return true;
}

static inline v8::MaybeLocal<v8::Object> ReadRawValue(v8::Local<v8::Context> ctx, v8::ValueDeserializer& deserializer)
{
    CProfiler::Sample _("ReadRawValue", true);
    v8::Isolate* isolate = ctx->GetIsolate();
    V8ResourceImpl* resource = V8ResourceImpl::Get(ctx);

    RawValueType* typePtr;
    if(!deserializer.ReadRawBytes(sizeof(uint8_t), (const void**)&typePtr)) return v8::MaybeLocal<v8::Object>();
    RawValueType type = *typePtr;

    switch(type)
    {
        case RawValueType::BASEOBJECT:
        {
            uint32_t* id;
            alt::IBaseObject::Type* type;
            bool* remote;
            if(!deserializer.ReadRawBytes(sizeof(uint32_t), (const void**)&id) || 
               !deserializer.ReadRawBytes(sizeof(uint8_t), (const void**)&type) ||
               !deserializer.ReadRawBytes(sizeof(bool), (const void**)&remote))
                return v8::MaybeLocal<v8::Object>();

            alt::IBaseObject* object;
#ifdef ALT_CLIENT_API
            if (*remote)
            {
                object = alt::ICore::Instance().GetBaseObjectByRemoteID(*type, *id);
            }
            else
            {
                object = alt::ICore::Instance().GetBaseObjectByID(*type, *id);
            }
#else
            if (*type == alt::IBaseObject::Type::LOCAL_PLAYER) *type = alt::IBaseObject::Type::PLAYER;
            object = alt::ICore::Instance().GetBaseObjectByID(*type, *id);
#endif // ALT_CLIENT_API
            if(!object) return v8::MaybeLocal<v8::Object>();
            return V8ResourceImpl::Get(ctx)->GetOrCreateEntity(object, "BaseObject")->GetJSVal(isolate);
        }
        case RawValueType::VECTOR3:
        {
            float* x;
            float* y;
            float* z;
            if(!deserializer.ReadRawBytes(sizeof(float), (const void**)&x) || !deserializer.ReadRawBytes(sizeof(float), (const void**)&y) ||
               !deserializer.ReadRawBytes(sizeof(float), (const void**)&z))
                return v8::MaybeLocal<v8::Object>();
            return resource->CreateVector3({ *x, *y, *z }).As<v8::Object>();
        }
        case RawValueType::VECTOR2:
        {
            float* x;
            float* y;
            if(!deserializer.ReadRawBytes(sizeof(float), (const void**)&x) || !deserializer.ReadRawBytes(sizeof(float), (const void**)&y)) return v8::MaybeLocal<v8::Object>();
            return resource->CreateVector2({ *x, *y }).As<v8::Object>();
        }
        case RawValueType::RGBA:
        {
            uint8_t* r;
            uint8_t* g;
            uint8_t* b;
            uint8_t* a;
            if(!deserializer.ReadRawBytes(sizeof(uint8_t), (const void**)&r) || !deserializer.ReadRawBytes(sizeof(uint8_t), (const void**)&g) ||
               !deserializer.ReadRawBytes(sizeof(uint8_t), (const void**)&b) || !deserializer.ReadRawBytes(sizeof(uint8_t), (const void**)&a))
                return v8::MaybeLocal<v8::Object>();
            return resource->CreateRGBA({ *r, *g, *b, *a }).As<v8::Object>();
        }
        default:
        {
            Log::Error << "Reading unknown raw value type: " << (int)type << Log::Endl;
            break;
        }
    }
    return v8::MaybeLocal<v8::Object>();
}

class WriteDelegate : public v8::ValueSerializer::Delegate
{
    v8::ValueSerializer* serializer;

public:
    WriteDelegate() {}

    void SetSerializer(v8::ValueSerializer* _serializer)
    {
        serializer = _serializer;
    }

    void ThrowDataCloneError(v8::Local<v8::String> message) override
    {
        V8Helpers::Throw(v8::Isolate::GetCurrent(), V8Helpers::CppValue(message));
    }

    v8::Maybe<bool> WriteHostObject(v8::Isolate* isolate, v8::Local<v8::Object> object) override
    {
        v8::Local<v8::Context> ctx = isolate->GetEnteredOrMicrotaskContext();
        RawValueType type = GetValueType(ctx, object);
        if(type == RawValueType::INVALID) return v8::Nothing<bool>();
        bool result = WriteRawValue(ctx, *serializer, type, object);
        if(!result)
        {
            V8Helpers::Throw(isolate, "Tried to write invalid raw value (functions or promises are not supported)");
            return v8::Nothing<bool>();
        }
        return v8::Just(true);
    }
};

class ReadDelegate : public v8::ValueDeserializer::Delegate
{
    v8::ValueDeserializer* deserializer;

public:
    ReadDelegate() {}

    void SetDeserializer(v8::ValueDeserializer* _deserializer)
    {
        deserializer = _deserializer;
    }

    v8::MaybeLocal<v8::Object> ReadHostObject(v8::Isolate* isolate) override
    {
        v8::Local<v8::Context> ctx = isolate->GetEnteredOrMicrotaskContext();
        return ReadRawValue(ctx, *deserializer);
    }
};

// Converts a JS value to a MValue byte array
alt::MValueByteArray V8Helpers::V8ToRawBytes(v8::Local<v8::Value> val)
{
    CProfiler::Sample _("V8Helpers::V8ToRawBytes", true);
    static WriteDelegate delegate;

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> ctx = isolate->GetEnteredOrMicrotaskContext();

    RawValueType type = GetValueType(ctx, val);
    if(type == RawValueType::INVALID) return alt::MValueByteArray();

    v8::ValueSerializer serializer(isolate, &delegate);
    delegate.SetSerializer(&serializer);

    serializer.WriteHeader();

    // Write the magic bytes to the buffer
    serializer.WriteRawBytes(magicBytes, sizeof(magicBytes));

    // Write the serialized value to the buffer
    bool result;
    if(!serializer.WriteValue(ctx, val).To(&result) || !result) return alt::MValueByteArray();

    std::pair<uint8_t*, size_t> serialized = serializer.Release();
    return alt::ICore::Instance().CreateMValueByteArray(serialized.first, serialized.second);
}

// Converts a MValue byte array to a JS value
v8::MaybeLocal<v8::Value> V8Helpers::RawBytesToV8(alt::MValueByteArrayConst rawBytes)
{
    CProfiler::Sample _("V8Helpers::RawBytesToV8", true);
    static ReadDelegate delegate;

    const uint8_t* data = rawBytes->GetData();
    size_t size = rawBytes->GetSize();
    if(size < sizeof(magicBytes)) return v8::MaybeLocal<v8::Value>();

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> ctx = isolate->GetEnteredOrMicrotaskContext();

    v8::ValueDeserializer deserializer(isolate, data, size, &delegate);
    delegate.SetDeserializer(&deserializer);

    bool headerValid;
    if(!deserializer.ReadHeader(ctx).To(&headerValid) || !headerValid) return v8::MaybeLocal<v8::Value>();

    // Check for magic bytes
    uint8_t* magicBytesPtr;
    if(!deserializer.ReadRawBytes(sizeof(magicBytes), (const void**)&magicBytesPtr)) return v8::MaybeLocal<v8::Value>();
    if(memcmp(magicBytesPtr, magicBytes, sizeof(magicBytes)) != 0) return v8::MaybeLocal<v8::Value>();

    // Deserialize the value
    v8::MaybeLocal<v8::Value> result = deserializer.ReadValue(ctx);

    return result;
}
