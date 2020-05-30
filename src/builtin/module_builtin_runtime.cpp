#include "daScript/misc/platform.h"

#include "module_builtin.h"

#include "daScript/ast/ast_interop.h"
#include "daScript/simulate/aot_builtin.h"
#include "daScript/simulate/runtime_profile.h"
#include "daScript/simulate/hash.h"
#include "daScript/simulate/bin_serializer.h"
#include "daScript/simulate/runtime_array.h"
#include "daScript/simulate/runtime_range.h"
#include "daScript/simulate/runtime_string_delete.h"
#include "daScript/simulate/simulate_nodes.h"

namespace das
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif

    struct MarkFunctionAnnotation : FunctionAnnotation {
        MarkFunctionAnnotation(const string & na) : FunctionAnnotation(na) { }
        virtual bool apply(ExprBlock *, ModuleGroup &, const AnnotationArgumentList &, string & err) override {
            err = "not supported for block";
            return false;
        }
        virtual bool finalize(ExprBlock *, ModuleGroup &,const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
        virtual bool finalize(const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
    };

    struct PrivateFunctionAnnotation : MarkFunctionAnnotation {
        PrivateFunctionAnnotation() : MarkFunctionAnnotation("private") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->privateFunction = true;
            return true;
        };
    };

    extern ProgramPtr g_Program;

    struct MacroFunctionAnnotation : MarkFunctionAnnotation {
        MacroFunctionAnnotation() : MarkFunctionAnnotation("_macro") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->init = true;
            g_Program->needMacroModule = true;
            return true;
        };
    };

    struct UnsafeDerefFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeDerefFunctionAnnotation() : MarkFunctionAnnotation("unsafe_deref") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafeDeref = true;
            return true;
        };
    };

    struct GenericFunctionAnnotation : MarkFunctionAnnotation {
        GenericFunctionAnnotation() : MarkFunctionAnnotation("generic") { }
        virtual bool isGeneric() const override {
            return true;
        }
        virtual bool apply(const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            return true;
        };
    };

    struct ExportFunctionAnnotation : MarkFunctionAnnotation {
        ExportFunctionAnnotation() : MarkFunctionAnnotation("export") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->exports = true;
            return true;
        };
    };

    struct SideEffectsFunctionAnnotation : MarkFunctionAnnotation {
        SideEffectsFunctionAnnotation() : MarkFunctionAnnotation("sideeffects") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->sideEffectFlags |= uint32_t(SideEffects::userScenario);
            return true;
        };
    };

    struct RunAtCompileTimeFunctionAnnotation : MarkFunctionAnnotation {
        RunAtCompileTimeFunctionAnnotation() : MarkFunctionAnnotation("run") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->hasToRunAtCompileTime = true;
            return true;
        };
    };

    struct UnsafeOpFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeOpFunctionAnnotation() : MarkFunctionAnnotation("unsafe_operation") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafeOperation = true;
            return true;
        };
    };

    struct UnsafeFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeFunctionAnnotation() : MarkFunctionAnnotation("unsafe") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafe = true;
            return true;
        };
    };

    struct NoAotFunctionAnnotation : MarkFunctionAnnotation {
        NoAotFunctionAnnotation() : MarkFunctionAnnotation("no_aot") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->noAot = true;
            return true;
        };
    };

    struct InitFunctionAnnotation : MarkFunctionAnnotation {
        InitFunctionAnnotation() : MarkFunctionAnnotation("init") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->init = true;
            return true;
        };
        virtual bool finalize(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string & errors) override {
            if ( func->arguments.size() ) {
                errors += "[init] function can't have any arguments";
                return false;
            }
            if ( !func->result->isVoid() ) {
                errors += "[init] function can't return value";
                return false;
            }
            return true;
        }
    };

    struct MarkUsedFunctionAnnotation : MarkFunctionAnnotation {
        MarkUsedFunctionAnnotation() : MarkFunctionAnnotation("unused_argument") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList & args, string &) override {
            for ( auto & fnArg : func->arguments ) {
                if ( auto optArg = args.find(fnArg->name, Type::tBool) ) {
                    fnArg->marked_used = optArg->bValue;
                }
            }
            return true;
        };
    };

    // totally dummy annotation, needed for comments
    struct CommentAnnotation : StructureAnnotation {
        CommentAnnotation() : StructureAnnotation("comment") {}
        virtual bool touch(const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
        virtual bool look ( const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
    };

    struct HybridFunctionAnnotation : MarkFunctionAnnotation {
        HybridFunctionAnnotation() : MarkFunctionAnnotation("hybrid") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->aotHybrid = true;
            return true;
        };
    };

    struct CppAlignmentAnnotation : StructureAnnotation {
        CppAlignmentAnnotation() : StructureAnnotation("cpp_layout") {}
        virtual bool touch(const StructurePtr & ps, ModuleGroup &,
                           const AnnotationArgumentList & args, string & ) override {
            ps->cppLayout = true;
            ps->cppLayoutNotPod = !args.getBoolOption("pod", true);
            return true;
        }
        virtual bool look ( const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
    };

    struct LocalOnlyFunctionAnnotation : FunctionAnnotation {
        LocalOnlyFunctionAnnotation() : FunctionAnnotation("local_only") { }
        virtual bool apply ( ExprBlock *, ModuleGroup &, const AnnotationArgumentList &, string & err ) override {
            err = "not a block annotation";
            return false;
        }
        virtual bool finalize ( ExprBlock *, ModuleGroup &, const AnnotationArgumentList &,
                               const AnnotationArgumentList &, string & err ) override {
            err = "not a block annotation";
            return false;
        }
        virtual bool apply ( const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, string & ) override {
            return true;
        };
        virtual bool finalize ( const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &,
                               const AnnotationArgumentList &, string & ) override {
            return true;
        }
        // [local_only ()]
        virtual bool verifyCall ( ExprCallFunc * call, const AnnotationArgumentList & args, string & err ) override {
            if ( !call->func ) {
                err = "unknown function";
                return false;
            }
            for ( size_t i=0; i!=call->func->arguments.size(); ++i ) {
                auto & farg = call->func->arguments[i];
                if ( auto it = args.find(farg->name, Type::tBool) ) {
                    auto & carg = call->arguments[i];
                    bool isLocalArg = carg->rtti_isMakeLocal() || carg->rtti_isMakeTuple();
                    bool isLocalFArg = it->bValue;
                    if ( isLocalArg != isLocalFArg ) {
                        err = isLocalFArg ? "expecting [[...]]" : "not expecting [[...]]";
                        return false;
                    }
                }
            }
            return true;
        }
    };

    struct PersistentStructureAnnotation : StructureAnnotation {
        PersistentStructureAnnotation() : StructureAnnotation("persistent") {}
        virtual bool touch(const StructurePtr & ps, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            ps->persistent = true;
            return true;
        }
        virtual bool look ( const StructurePtr & st, ModuleGroup &,
                           const AnnotationArgumentList & args, string & errors ) override {
            bool allPod = true;
            if ( !args.getBoolOption("mixed_heap", false) ) {
                for ( const auto & field : st->fields ) {
                    if ( !field.type->isPod() ) {
                        allPod = false;
                        errors += "\t" + field.name + " : " + field.type->describe() + " is not a pod\n";
                    }
                }
            }
            return allPod;
        }
    };


#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    template <typename intT>
    struct EnumIterator : Iterator {
        EnumIterator ( EnumInfo * ei ) : info(ei) {}
        virtual bool first ( Context &, char * _value ) override {
            count = 0;
            range_to = int32_t(info->count);
            if ( range_to ) {
                intT * value = (intT *) _value;
                *value      = intT(info->fields[count++]->value);
                return true;
            } else {
                return false;
            }
        }
        virtual bool next  ( Context &, char * _value ) override {
            if ( range_to != count ) {
                intT * value = (intT *) _value;
                *value = intT(info->fields[count++]->value);
                return true;
            } else {
                return false;
            }
        }
        virtual void close ( Context & context, char * ) override {
            context.heap.free((char *)this, sizeof(RangeIterator));
        }
        EnumInfo *  info = nullptr;
        int32_t     count = 0;
        int32_t     range_to = 0;
    };

    // core functions

    void builtin_throw ( char * text, Context * context ) {
        context->throw_error(text);
    }

    void builtin_print ( char * text, Context * context ) {
        context->to_out(text);
    }

    vec4f builtin_breakpoint ( Context & context, SimNode_CallBase * call, vec4f * ) {
        context.breakPoint(call->debugInfo);
        return v_zero();
    }

    void builtin_stackwalk ( Context * context, LineInfoArg * lineInfo ) {
        context->stackWalk(lineInfo, true, true);
    }

    void builtin_terminate ( Context * context ) {
        context->throw_error("terminate");
    }

    int builtin_table_size ( const Table & arr ) {
        return arr.size;
    }

    int builtin_table_capacity ( const Table & arr ) {
        return arr.capacity;
    }

    void builtin_table_clear ( Table & arr, Context * context ) {
        table_clear(*context, arr);
    }

    vec4f _builtin_hash ( Context & context, SimNode_CallBase * call, vec4f * args ) {
        auto uhash = hash_value(context, args[0], call->types[0]);
        return cast<uint32_t>::from(uhash);
    }

    uint32_t heap_bytes_allocated ( Context * context ) {
        return context->heap.bytesAllocated();
    }

    uint32_t heap_high_watermark ( Context * context ) {
        return (int32_t) context->heap.maxBytesAllocated();
    }

    int32_t heap_depth ( Context * context ) {
        return (int32_t) context->heap.shelf.size();
    }

    uint32_t string_heap_bytes_allocated ( Context * context ) {
        return context->stringHeap.bytesAllocated();
    }

    uint32_t string_heap_high_watermark ( Context * context ) {
        return (int32_t) context->stringHeap.maxBytesAllocated();
    }

    int32_t string_heap_depth ( Context * context ) {
        return (int32_t) context->stringHeap.shelf.size();
    }

    void string_heap_collect ( Context * context ) {
        context->collectStringHeap();
    }

    void string_heap_report ( Context * context ) {
        context->stringHeap.reportAllocations();
    }

    void builtin_table_lock ( const Table & arr, Context * context ) {
        table_lock(*context, const_cast<Table&>(arr));
    }

    void builtin_table_unlock ( const Table & arr, Context * context ) {
        table_unlock(*context, const_cast<Table&>(arr));
    }

    bool builtin_iterator_first ( const Sequence & it, void * data, Context * context ) {
        if ( !it.iter ) context->throw_error("calling first on empty iterator");
        else if ( it.iter->isOpen ) context->throw_error("calling first on already open iterator");
        it.iter->isOpen = true;
        return it.iter->first(*context, (char *)data);
    }

    bool builtin_iterator_next ( const Sequence & it, void * data, Context * context ) {
        if ( !it.iter ) context->throw_error("calling next on empty iterator");
        else if ( !it.iter->isOpen ) context->throw_error("calling next on a non-open iterator");
        return it.iter->next(*context, (char *)data);
    }

    void builtin_iterator_close ( const Sequence & it, void * data, Context * context ) {
        if ( it.iter ) {
            it.iter->close(*context, (char *)&data);
            ((Sequence&)it).iter = nullptr;
        }
    }

    void builtin_iterator_delete ( const Sequence & it, Context * context ) {
        if ( it.iter ) it.iter->close(*context, nullptr);
        ((Sequence&)it).iter = nullptr;
    }

    bool builtin_iterator_iterate ( const Sequence & it, void * value, Context * context ) {
        if ( !it.iter ) {
            return false;
        } else if ( !it.iter->isOpen) {
            if ( !it.iter->first(*context, (char *)value) ) {
                it.iter->close(*context, (char *)value);
                ((Sequence&)it).iter = nullptr;
                return false;
            } else {
                it.iter->isOpen = true;
                return true;
            }
        } else {
            if ( !it.iter->next(*context, (char *)value) ) {
                it.iter->close(*context, (char *)value);
                ((Sequence&)it).iter = nullptr;
                return false;
            } else {
                return true;
            }
        }
    }

    void builtin_make_good_array_iterator ( Sequence & result, const Array & arr, int stride, Context * context ) {
        char * iter = context->heap.allocate(sizeof(GoodArrayIterator));
        new (iter) GoodArrayIterator((Array *)&arr, stride);
        result = { (Iterator *) iter };
    }

    void builtin_make_fixed_array_iterator ( Sequence & result, void * data, int size, int stride, Context * context ) {
        char * iter = context->heap.allocate(sizeof(FixedArrayIterator));
        new (iter) FixedArrayIterator((char *)data, size, stride);
        result = { (Iterator *) iter };
    }

    void builtin_make_range_iterator ( Sequence & result, range rng, Context * context ) {
        char * iter = context->heap.allocate(sizeof(RangeIterator));
        new (iter) RangeIterator(rng);
        result = { (Iterator *) iter };
    }

    vec4f builtin_make_enum_iterator ( Context & context, SimNode_CallBase * call, vec4f * args ) {
        if ( !call->types ) context.throw_error("missing type info");
        auto itinfo = call->types[0];
        if ( itinfo->type != Type::tIterator ) context.throw_error("not an iterator");
        auto tinfo = itinfo->firstType;
        if ( !tinfo ) context.throw_error("missing iterator type info");
        if ( tinfo->type!=Type::tEnumeration && tinfo->type!=Type::tEnumeration8
            && tinfo->type!=Type::tEnumeration16 ) {
            context.throw_error("not an iterator of enumeration");
        }
        auto einfo = tinfo->enumType;
        if ( !einfo ) context.throw_error("missing enumeraiton type info");
        char * iter = nullptr;
        switch ( tinfo->type ) {
        case Type::tEnumeration:
            iter = context.heap.allocate(sizeof(EnumIterator<int32_t>));
            new (iter) EnumIterator<int32_t>(einfo);
            break;
        case Type::tEnumeration8:
            iter = context.heap.allocate(sizeof(EnumIterator<int8_t>));
            new (iter) EnumIterator<int8_t>(einfo);
            break;
        case Type::tEnumeration16:
            iter = context.heap.allocate(sizeof(EnumIterator<int16_t>));
            new (iter) EnumIterator<int16_t>(einfo);
            break;
        default:
            DAS_ASSERT(0 && "how???");
        }
        Sequence * seq = cast<Sequence *>::to(args[0]);
        seq->iter = (Iterator *) iter;
        return v_zero();
    }

    void builtin_make_string_iterator ( Sequence & result, char * str, Context * context ) {
        char * iter = context->heap.allocate(sizeof(StringIterator));
        new (iter) StringIterator(str);
        result = { (Iterator *) iter };
    }

    struct NilIterator : Iterator {
        virtual bool first ( Context &, char * ) override { return false; }
        virtual bool next  ( Context &, char * ) override { return false; }
        virtual void close ( Context & context, char * ) override {
            context.heap.free((char *)this, sizeof(NilIterator));
        }
    };

    void builtin_make_nil_iterator ( Sequence & result, Context * context ) {
        char * iter = context->heap.allocate(sizeof(NilIterator));
        new (iter) NilIterator();
        result = { (Iterator *) iter };
    }

    struct LambdaIterator : Iterator {
        LambdaIterator ( Context & context, const Lambda & ll ) : lambda(ll) {
            int32_t * fnIndex = (int32_t *) lambda.capture;
            if (!fnIndex) context.throw_error("invoke null lambda");
            simFunc = context.getFunction(*fnIndex-1);
            if (!simFunc) context.throw_error("invoke null function");
        }
        __forceinline bool InvokeLambda ( Context & context, char * ptr ) {
            vec4f argValues[4] = {
                cast<Lambda>::from(lambda),
                cast<char *>::from(ptr)
            };
            auto res = context.call(simFunc, argValues, 0);
            return cast<bool>::to(res);
        }
        virtual bool first ( Context & context, char * ptr ) override {
            return InvokeLambda(context, ptr);
        }
        virtual bool next  ( Context & context, char * ptr ) override {
            return InvokeLambda(context, ptr);
        }
        virtual void close ( Context & context, char * ) override {
            int32_t * fnIndex = (int32_t *) lambda.capture;
            SimFunction * finFunc = context.getFunction(fnIndex[1]-1);
            if (!finFunc) context.throw_error("generator finalizer is a null function");
            vec4f argValues[1] = {
                cast<void *>::from(lambda.capture)
            };
            auto flags = context.stopFlags; // need to save stop flags, we can be in the middle of some return or something
            context.call(finFunc, argValues, 0);
            context.heap.free((char *)this, sizeof(LambdaIterator));
            context.stopFlags = flags;
        }
        virtual void walk ( DataWalker & walker ) override {
            walker.beforeLambda(&lambda, lambda.getTypeInfo());
            walker.walk(lambda.capture, lambda.getTypeInfo());
            walker.afterLambda(&lambda, lambda.getTypeInfo());
        }
        Lambda          lambda;
        SimFunction *   simFunc = nullptr;
    };

    void builtin_make_lambda_iterator ( Sequence & result, const Lambda lambda, Context * context ) {
        char * iter = context->heap.allocate(sizeof(LambdaIterator));
        new (iter) LambdaIterator(*context, lambda);
        result = { (Iterator *) iter };
    }

    void resetProfiler( Context * context ) {
        context->resetProfiler();
    }

    void dumpProfileInfo( Context * context ) {
        context->dumpProfileInfo();
    }

    void builtin_array_free ( Array & dim, int szt, Context * __context__ ) {
        if ( dim.data ) {
            if ( !dim.lock ) {
                uint32_t oldSize = dim.capacity*szt;
                __context__->heap.free(dim.data, oldSize);
            } else {
                __context__->throw_error("can't delete locked array");
            }
        }
        memset ( &dim, 0, sizeof(Array) );
    }

    void builtin_table_free ( Table & tab, int szk, int szv, Context * __context__ ) {
        if ( tab.data ) {
            if ( !tab.lock ) {
                uint32_t oldSize = tab.capacity*(szk+szv+sizeof(uint32_t));
                __context__->heap.free(tab.data, oldSize);
            } else {
                __context__->throw_error("can't delete locked table");
            }
        }
        memset ( &tab, 0, sizeof(Table) );
    }

    void builtin_smart_ptr_clone_ptr ( smart_ptr_raw<void> & dest, const void * src ) {
        ptr_ref_count * t = (ptr_ref_count *) dest.ptr;
        dest.ptr = (void *) src;
        if ( src ) ((ptr_ref_count *) src)->addRef();
        if ( t ) t->delRef();
    }

    void builtin_smart_ptr_clone ( smart_ptr_raw<void> & dest, const smart_ptr_raw<void> src ) {
        ptr_ref_count * t = (ptr_ref_count *) dest.ptr;
        dest.ptr = src.ptr;
        if ( src.ptr ) ((ptr_ref_count *) src.ptr)->addRef();
        if ( t ) t->delRef();
    }

    uint32_t builtin_smart_ptr_use_count ( const smart_ptr_raw<void> src ) {
        ptr_ref_count * psrc = (ptr_ref_count *) src.ptr;
        return psrc ? psrc->use_count() : 0;
    }

    struct ClassInfoMacro : TypeInfoMacro {
        ClassInfoMacro() : TypeInfoMacro("rtti_classinfo") {}
        virtual TypeDeclPtr getAstType ( ModuleLibrary & lib, const ExpressionPtr &, string & ) override {
            return typeFactory<void *>::make(lib);
        }
        virtual SimNode * simluate ( Context * context, const ExpressionPtr & expr, string & )  override {
            auto exprTypeInfo = static_pointer_cast<ExprTypeInfo>(expr);
            TypeInfo * typeInfo = context->thisHelper->makeTypeInfo(nullptr, exprTypeInfo->typeexpr);
            return context->code->makeNode<SimNode_TypeInfo>(expr->at, typeInfo);
        }
        virtual void aotPrefix ( TextWriter & ss, const ExpressionPtr & ) override {
            ss << "(void *)(&";
        }
        virtual void aotSuffix ( TextWriter & ss, const ExpressionPtr & ) override {
            ss << ")";
        }
        virtual bool aotNeedTypeInfo ( const ExpressionPtr & ) const override {
            return true;
        }
    };

    bool is_compiling ( Context * ctx ) {
        if ( !ctx->thisProgram ) return false;
        return ctx->thisProgram->isCompiling || ctx->thisProgram->isSimulating;
    }

    bool is_compiling_macros ( Context * ctx ) {
        if ( !ctx->thisProgram ) return false;
        return ctx->thisProgram->isCompilingMacros;
    }

    void Module_BuiltIn::addRuntime(ModuleLibrary & lib) {
        // function annotations
        addAnnotation(make_smart<CommentAnnotation>());
        addAnnotation(make_smart<CppAlignmentAnnotation>());
        addAnnotation(make_smart<GenericFunctionAnnotation>());
        addAnnotation(make_smart<PrivateFunctionAnnotation>());
        addAnnotation(make_smart<MacroFunctionAnnotation>());
        addAnnotation(make_smart<ExportFunctionAnnotation>());
        addAnnotation(make_smart<SideEffectsFunctionAnnotation>());
        addAnnotation(make_smart<RunAtCompileTimeFunctionAnnotation>());
        addAnnotation(make_smart<UnsafeFunctionAnnotation>());
        addAnnotation(make_smart<UnsafeOpFunctionAnnotation>());
        addAnnotation(make_smart<NoAotFunctionAnnotation>());
        addAnnotation(make_smart<InitFunctionAnnotation>());
        addAnnotation(make_smart<HybridFunctionAnnotation>());
        addAnnotation(make_smart<UnsafeDerefFunctionAnnotation>());
        addAnnotation(make_smart<MarkUsedFunctionAnnotation>());
        addAnnotation(make_smart<LocalOnlyFunctionAnnotation>());
        addAnnotation(make_smart<PersistentStructureAnnotation>());
        // typeinfo macros
        addTypeInfoMacro(make_smart<ClassInfoMacro>());
        // compile-time functions
        addExtern<DAS_BIND_FUN(is_compiling)>(*this, lib, "is_compiling",
            SideEffects::accessExternal, "is_compiling");
        addExtern<DAS_BIND_FUN(is_compiling_macros)>(*this, lib, "is_compiling_macros",
            SideEffects::accessExternal, "is_compiling_macros");
        // iterator functions
        addExtern<DAS_BIND_FUN(builtin_iterator_first)>(*this, lib, "_builtin_iterator_first",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_first");
        addExtern<DAS_BIND_FUN(builtin_iterator_next)>(*this, lib,  "_builtin_iterator_next",
                                                       SideEffects::modifyArgumentAndExternal, "builtin_iterator_next");
        addExtern<DAS_BIND_FUN(builtin_iterator_close)>(*this, lib, "_builtin_iterator_close",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_close");
        addExtern<DAS_BIND_FUN(builtin_iterator_delete)>(*this, lib, "_builtin_iterator_delete",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_delete");
        addExtern<DAS_BIND_FUN(builtin_iterator_iterate)>(*this, lib, "_builtin_iterator_iterate",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_iterate");
        // make-iterator functions
        addExtern<DAS_BIND_FUN(builtin_make_good_array_iterator)>(*this, lib,  "_builtin_make_good_array_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_good_array_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_fixed_array_iterator)>(*this, lib,  "_builtin_make_fixed_array_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_fixed_array_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_range_iterator)>(*this, lib,  "_builtin_make_range_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_range_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_string_iterator)>(*this, lib,  "_builtin_make_string_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_string_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_nil_iterator)>(*this, lib,  "_builtin_make_nil_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_nil_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_lambda_iterator)>(*this, lib,  "_builtin_make_lambda_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_lambda_iterator");
        addInterop<builtin_make_enum_iterator,void,vec4f>(*this, lib, "_builtin_make_enum_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_enum_iterator");
        // functions
        addExtern<DAS_BIND_FUN(builtin_throw)>         (*this, lib, "panic", SideEffects::modifyExternal, "builtin_throw");
        addExtern<DAS_BIND_FUN(builtin_print)>         (*this, lib, "print", SideEffects::modifyExternal, "builtin_print");
        addExtern<DAS_BIND_FUN(builtin_terminate)> (*this, lib, "terminate", SideEffects::modifyExternal, "terminate");
        addExtern<DAS_BIND_FUN(builtin_stackwalk)> (*this, lib, "stackwalk", SideEffects::modifyExternal, "builtin_stackwalk");
        addInterop<builtin_breakpoint,void>     (*this, lib, "breakpoint", SideEffects::modifyExternal, "breakpoint");
        // profiler
        addExtern<DAS_BIND_FUN(resetProfiler)>(*this, lib, "reset_profiler", SideEffects::modifyExternal, "resetProfiler");
        addExtern<DAS_BIND_FUN(dumpProfileInfo)>(*this, lib, "dump_profile_info", SideEffects::modifyExternal, "dumpProfileInfo");
        // variant
        addExtern<DAS_BIND_FUN(variant_index)>(*this, lib, "variant_index", SideEffects::none, "variant_index");
        auto svi = addExtern<DAS_BIND_FUN(set_variant_index)>(*this, lib, "set_variant_index",
            SideEffects::modifyArgument, "set_variant_index");
        svi->unsafeOperation = true;
        // heap
        addExtern<DAS_BIND_FUN(heap_bytes_allocated)>(*this, lib, "heap_bytes_allocated",
                SideEffects::modifyExternal, "heap_bytes_allocated");
        addExtern<DAS_BIND_FUN(heap_high_watermark)>(*this, lib, "heap_high_watermark",
                SideEffects::modifyExternal, "heap_high_watermark");
        addExtern<DAS_BIND_FUN(heap_depth)>(*this, lib, "heap_depth",
                SideEffects::modifyExternal, "heap_depth");
        addExtern<DAS_BIND_FUN(string_heap_bytes_allocated)>(*this, lib, "string_heap_bytes_allocated",
                SideEffects::modifyExternal, "string_heap_bytes_allocated");
        addExtern<DAS_BIND_FUN(string_heap_high_watermark)>(*this, lib, "string_heap_high_watermark",
                SideEffects::modifyExternal, "string_heap_high_watermark");
        addExtern<DAS_BIND_FUN(string_heap_depth)>(*this, lib, "string_heap_depth",
                SideEffects::modifyExternal, "string_heap_depth");
        auto shc = addExtern<DAS_BIND_FUN(string_heap_collect)>(*this, lib, "string_heap_collect",
                SideEffects::modifyExternal, "string_heap_collect");
        shc->unsafeOperation = true;
        addExtern<DAS_BIND_FUN(string_heap_report)>(*this, lib, "string_heap_report",
                SideEffects::modifyExternal, "string_heap_report");
        // binary serializer
        addInterop<_builtin_binary_load,void,vec4f,const Array &>(*this,lib,"_builtin_binary_load",SideEffects::modifyArgumentAndExternal, "_builtin_binary_load");
        addInterop<_builtin_binary_save,void,const vec4f,const Block &>(*this, lib, "_builtin_binary_save",SideEffects::modifyExternal, "_builtin_binary_save");
        // function-like expresions
        addCall<ExprAssert>         ("assert",false);
        addCall<ExprAssert>         ("verify",true);
        addCall<ExprStaticAssert>   ("static_assert");
        addCall<ExprStaticAssert>   ("concept_assert");
        addCall<ExprDebug>          ("debug");
        addCall<ExprMemZero>        ("memzero");
        // hash
        addInterop<_builtin_hash,uint32_t,vec4f>(*this, lib, "hash", SideEffects::none, "hash");
        // table functions
        addExtern<DAS_BIND_FUN(builtin_table_clear)>(*this, lib, "clear", SideEffects::modifyArgument, "builtin_table_clear");
        addExtern<DAS_BIND_FUN(builtin_table_size)>(*this, lib, "length", SideEffects::none, "builtin_table_size");
        addExtern<DAS_BIND_FUN(builtin_table_capacity)>(*this, lib, "capacity", SideEffects::none, "builtin_table_capacity");
        addExtern<DAS_BIND_FUN(builtin_table_lock)>(*this, lib, "__builtin_table_lock",
                                                    SideEffects::modifyArgumentAndExternal, "builtin_table_lock");
        addExtern<DAS_BIND_FUN(builtin_table_unlock)>(*this, lib, "__builtin_table_unlock",
                                                      SideEffects::modifyArgumentAndExternal, "builtin_table_unlock");
        addExtern<DAS_BIND_FUN(builtin_table_keys)>(*this, lib, "__builtin_table_keys",
                                                    SideEffects::modifyArgumentAndExternal, "builtin_table_keys");
        addExtern<DAS_BIND_FUN(builtin_table_values)>(*this, lib, "__builtin_table_values",
                                                      SideEffects::modifyArgumentAndExternal, "builtin_table_values");
        // array and table free
        addExtern<DAS_BIND_FUN(builtin_array_free)>(*this, lib, "builtin_array_free", SideEffects::modifyArgumentAndExternal, "builtin_array_free");
        addExtern<DAS_BIND_FUN(builtin_table_free)>(*this, lib, "builtin_table_free", SideEffects::modifyArgumentAndExternal, "builtin_table_free");
        // table expressions
        addCall<ExprErase>("__builtin_table_erase");
        addCall<ExprFind>("__builtin_table_find");
        addCall<ExprKeyExists>("__builtin_table_key_exists");
        // blocks
        addCall<ExprInvoke>("invoke");
        // smart ptr stuff
        addExtern<DAS_BIND_FUN(builtin_smart_ptr_clone_ptr)>(*this, lib, "smart_ptr_clone", SideEffects::modifyExternal, "builtin_smart_ptr_clone_ptr");
        addExtern<DAS_BIND_FUN(builtin_smart_ptr_clone)>(*this, lib, "smart_ptr_clone", SideEffects::modifyExternal, "builtin_smart_ptr_clone");
        addExtern<DAS_BIND_FUN(builtin_smart_ptr_use_count)>(*this, lib, "smart_ptr_use_count", SideEffects::none, "builtin_smart_ptr_use_count");
        addExtern<DAS_BIND_FUN(equ_sptr_sptr)>(*this, lib, "==", SideEffects::none, "equ_sptr_sptr");
        addExtern<DAS_BIND_FUN(nequ_sptr_sptr)>(*this, lib, "!=", SideEffects::none, "nequ_sptr_sptr");
        addExtern<DAS_BIND_FUN(equ_ptr_sptr)>(*this, lib, "==", SideEffects::none, "equ_ptr_sptr");
        addExtern<DAS_BIND_FUN(nequ_ptr_sptr)>(*this, lib, "!=", SideEffects::none, "nequ_ptr_sptr");
        addExtern<DAS_BIND_FUN(equ_sptr_ptr)>(*this, lib, "==", SideEffects::none, "equ_sptr_ptr");
        addExtern<DAS_BIND_FUN(nequ_sptr_ptr)>(*this, lib, "!=", SideEffects::none, "nequ_sptr_ptr");
        // profile
        addExtern<DAS_BIND_FUN(builtin_profile)>(*this,lib,"profile", SideEffects::modifyExternal, "builtin_profile");
        addString(lib);
    }
}
