#pragma once
// Codegen markers. These expand to a clang::annotate string ONLY when the codegen tool parses the
// header (it defines TBX_CODEGEN); every normal MSVC/clang build expands them to nothing, so they cost
// nothing at runtime and never warn. clang::annotate is used (not a raw [[tbx::...]] attribute) because
// libclang reliably surfaces annotate attributes in the AST while dropping unknown custom attributes.
//
// Usage:
//   struct TBX_SERIALIZABLE() TBX_DLL_EXPORT Transform : Block { ... };              // reflect-only
//   struct TBX_SERIALIZABLE(SerializerFormat::DEFAULT) ... Material : Asset { ... }; // + serializer
//   struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&read_tex, writer=&write_tex) ... Texture;
//   struct TBX_SERIALIZABLE(SerializerFormat::TEXT, name="UiDocument") ... Document;
//   float TBX_DO_NOT_SERIALIZE cached;                                               // reflected, off-disk
//   TBX_EXPOSED_TO_SCRIPTING Quat angle_axis(float radians, const Vec3& axis);       // scripting binding
//
// TBX_SERIALIZABLE is variadic (a string literal is one preprocessing token, so commas inside a quoted
// arg do not split it). First positional arg is the SerializerFormat:: enum; then named reader=&fn,
// writer=&fn, version=N, name="WireName". No args => reflect-only (register_type, no register_serializer).

#ifdef TBX_CODEGEN
    #define TBX_SERIALIZABLE(...) [[clang::annotate("tbx::serializable(" #__VA_ARGS__ ")")]]
    #define TBX_DO_NOT_SERIALIZE [[clang::annotate("tbx::do_not_serialize")]]
    #define TBX_EXPOSED_TO_SCRIPTING [[clang::annotate("tbx::exposed_to_scripting")]]
#else
    #define TBX_SERIALIZABLE(...)
    #define TBX_DO_NOT_SERIALIZE
    #define TBX_EXPOSED_TO_SCRIPTING
#endif
