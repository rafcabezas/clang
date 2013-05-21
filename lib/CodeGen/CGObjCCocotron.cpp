//===------- CGObjCCocotron.cpp - Emit LLVM Code from ASTs for a Module --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides Objective-C code generation targeting the GNU runtime.  The
// class in this file generates structures used by the GNU Objective-C runtime
// library.  These structures are defined in objc/objc.h and objc/objc-api.h in
// the GNU runtime distribution.
//
//===----------------------------------------------------------------------===//


//using llvm::dyn_cast;

#ifdef __INCLUDED_FROM_CGOBJCGNU_CPP__
#ifdef __IN_ANONYMOUS_NAMESPACE__

/// Cocotron runtime code generation.  This class implements the parts of
/// Objective-C support that are specific to the stock Cocotron runtime
class CGObjCCocotron : public CGObjCGNU {
    /// The GCC ABI message lookup function.  Returns an IMP pointing to the
    /// method implementation for this message.
    LazyRuntimeFunction MsgLookupFn;
    /// The GCC ABI superclass message lookup function.  Takes a pointer to a
    /// structure describing the receiver and the class, and a selector as
    /// arguments.  Returns the IMP for the corresponding method.
    LazyRuntimeFunction MsgLookupSuperFn;
protected:
    /// CacheTy - LLVM type for struct objc_cache.
    llvm::Type *CacheTy;
    /// CachePtrTy - LLVM type for struct objc_cache *.
    llvm::Type *CachePtrTy;
protected:
    virtual llvm::Value *LookupIMP(CodeGenFunction &CGF,
                                   llvm::Value *&Receiver,
                                   llvm::Value *cmd,
                                   llvm::MDNode *node) {
        CGBuilderTy &Builder = CGF.Builder;
        llvm::Value *args[] = {
            EnforceType(Builder, Receiver, IdTy),
            EnforceType(Builder, cmd, SelectorTy) };
        llvm::CallSite imp = CGF.EmitCallOrInvoke(MsgLookupFn, args);
        imp->setMetadata(msgSendMDKind, node);
        return imp.getInstruction();
    }
    virtual llvm::Value *LookupIMPSuper(CodeGenFunction &CGF,
                                        llvm::Value *ObjCSuper,
                                        llvm::Value *cmd) {
        CGBuilderTy &Builder = CGF.Builder;
        llvm::Value *lookupArgs[] = {EnforceType(Builder, ObjCSuper,
                                                 PtrToObjCSuperTy), cmd};
        return Builder.CreateCall(MsgLookupSuperFn, lookupArgs);
    }
    
    /// Returns a selector with the specified type encoding.  An empty string is
    /// used to return an untyped selector (with the types field set to NULL).
    llvm::Value *GetSelector(CGBuilderTy &Builder, Selector Sel,
                             const std::string &TypeEncoding, bool lval);
    
    /// Generates a class structure.
    llvm::Constant *GenerateClassStructure(
                                           llvm::Constant *MetaClass,
                                           llvm::Constant *SuperClass,
                                           unsigned info,
                                           const char *Name,
                                           llvm::Constant *Version,
                                           llvm::Constant *InstanceSize,
                                           llvm::Constant *IVars,
                                           llvm::Constant *Methods,
                                           llvm::Constant *Protocols,
                                           llvm::Constant *IvarOffsets,
                                           llvm::Constant *Properties,
                                           llvm::Constant *StrongIvarBitmap,
                                           llvm::Constant *WeakIvarBitmap,
                                           bool isMeta=false);
    
    llvm::Constant *GenerateClassStructureOld(
                                              llvm::Constant *MetaClass,
                                              llvm::Constant *SuperClass,
                                              unsigned info,
                                              const char *Name,
                                              llvm::Constant *Version,
                                              llvm::Constant *InstanceSize,
                                              llvm::Constant *IVars,
                                              llvm::Constant *Methods,
                                              llvm::Constant *Protocols,
                                              llvm::Constant *IvarOffsets,
                                              llvm::Constant *Properties,
                                              bool isMeta = false);

    llvm::Constant *GenerateEmptyProtocol(const std::string &ProtocolName);

public:
    CGObjCCocotron(CodeGenModule &Mod) : CGObjCGNU(Mod, 0, 0) {
        // IMP objc_msg_lookup(id, SEL);
        MsgLookupFn.init(&CGM, "objc_msg_lookup", IMPTy, IdTy, SelectorTy, NULL);
        // IMP objc_msg_lookup_super(struct objc_super*, SEL);
        MsgLookupSuperFn.init(&CGM, "objc_msg_lookup_super", IMPTy,
                              PtrToObjCSuperTy, SelectorTy, NULL);
        
        CacheTy = llvm::StructType/*OpaqueType*/::get(VMContext);
        //CGM.getModule().addTypeName("struct._objc_cache", CacheTy);
        //CacheTy->setName("struct._objc_cache");
        CGM.getModule().getOrInsertGlobal("struct._objc_cache", CacheTy);
        CachePtrTy = llvm::PointerType::getUnqual(CacheTy);

    }
    llvm::Constant *GenerateConstantString(const StringLiteral *);
    llvm::Value *EmitIvarOffset(CodeGenFunction &CGF,
                                const ObjCInterfaceDecl *Interface,
                                const ObjCIvarDecl *Ivar);
    void GenerateClass(const ObjCImplementationDecl *ClassDecl);
};


#else

/*
llvm::Value *CGObjCCocotron::GetClassNamed(CGBuilderTy &Builder,
                                      const std::string &Name,
                                      bool isWeak) {
    llvm::Value *ClassName = CGM.GetAddrOfConstantCString(Name);
    // With the incompatible ABI, this will need to be replaced with a direct
    // reference to the class symbol.  For the compatible nonfragile ABI we are
    // still performing this lookup at run time but emitting the symbol for the
    // class externally so that we can make the switch later.
    //
    // Libobjc2 contains an LLVM pass that replaces calls to objc_lookup_class
    // with memoized versions or with static references if it's safe to do so.
    if (!isWeak)
        EmitClassRef(Name);
    ClassName = Builder.CreateStructGEP(ClassName, 0);
    
    llvm::Constant *ClassLookupFn =
    CGM.CreateRuntimeFunction(llvm::FunctionType::get(IdTy, PtrToInt8Ty, true),
                              "objc_lookUpClass");
    return Builder.CreateCall(ClassLookupFn, ClassName);
}
*/

llvm::Value *CGObjCCocotron::GetSelector(CGBuilderTy &Builder, Selector Sel,
                                    const std::string &TypeEncoding, bool lval) {
    
#if 0
    llvm::GlobalAlias *&SelValue =  SelectorTable[Sel];
    
    if (0 == SelValue) {
        SelValue = new llvm::GlobalAlias(PtrToInt8Ty,
                                         llvm::GlobalValue::PrivateLinkage,
                                         ".objc_selector_alias_"+Sel.getAsString(), NULL,
                                         &TheModule);
        
    }
#endif
#if 0
    SmallVector<TypedSelector, 2> &Types = SelectorTable[Sel];
    llvm::GlobalAlias *SelValue = 0;
    
    
    for (SmallVectorImpl<TypedSelector>::iterator i = Types.begin(),
         e = Types.end() ; i!=e ; i++) {
        if (i->first == TypeEncoding) {
            SelValue = i->second;
            break;
        }
    }
    if (0 == SelValue) {
        SelValue = new llvm::GlobalAlias(SelectorTy,
                                         llvm::GlobalValue::PrivateLinkage,
                                         ".objc_selector_alias_"+Sel.getAsString(), NULL,
                                         &TheModule);
        Types.push_back(TypedSelector(TypeEncoding, SelValue));
    }
#endif
    //TODO FIX IT
    
    /*
    if (lval) {
        llvm::Value *tmp = Builder.CreateAlloca(SelValue->getType());
        Builder.CreateStore(SelValue, tmp);
        return tmp;
    }
    return SelValue;
     */
    
    llvm::Value *selectorString = CGM.GetAddrOfConstantCString(Sel.getAsString());
    selectorString = Builder.CreateStructGEP(selectorString, 0);
    
    std::vector<const llvm::Type*> Params(1, PtrToInt8Ty);
    llvm::Constant *sel_getUidFn =
    CGM.CreateRuntimeFunction(llvm::FunctionType::get(IdTy, PtrToInt8Ty, true),
                              "sel_getUid");
    
    return Builder.CreateCall(sel_getUidFn, selectorString);
}

#if 1 //Old
/// Generate an NSConstantString object.
llvm::Constant *CGObjCCocotron::GenerateConstantString(const StringLiteral *SL) {
    std::string Str = SL->getString().str();

    // Look for an existing one
    llvm::StringMap<llvm::Constant*>::iterator old = ObjCStrings.find(Str);
    if (old != ObjCStrings.end())
        return old->getValue();
    
    llvm::Constant *Zero =
    llvm::Constant::getNullValue(llvm::Type::getInt32Ty(VMContext));
    llvm::Constant *Zeros[] = { Zero, Zero };
    
    llvm::Type *Ty = CGM.getTypes().ConvertType(CGM.getContext().IntTy);
    Ty = llvm::ArrayType::get(Ty, 0);
    
    llvm::Constant *GV;
    GV = CGM.CreateRuntimeVariable(Ty, "_NSConstantStringClassReference");
    
    llvm::Constant *ConstantStringClassRef = llvm::ConstantExpr::getGetElementPtr(GV, Zeros);    
    
    std::vector<llvm::Constant*> Ivars;
    //Ivars.push_back(llvm::ConstantExpr::getGetElementPtr(GV, Zeros, 2));
    Ivars.push_back(ConstantStringClassRef);
    Ivars.push_back(MakeConstantString(Str));
    Ivars.push_back(llvm::ConstantInt::get(IntTy, Str.size()));
    llvm::Constant *ObjCStr = MakeGlobal(
                                         llvm::StructType::get(PtrToIntTy, PtrToInt8Ty, IntTy, NULL),
                                         Ivars, ".objc_str");
    ObjCStr = llvm::ConstantExpr::getBitCast(ObjCStr, PtrToInt8Ty);
    ObjCStrings[Str] = ObjCStr;
    ConstantStrings.push_back(ObjCStr);
    return ObjCStr;
}
#else //New
/// Generate an NSConstantString object.
llvm::Constant *CGObjCCocotron::GenerateConstantString(const StringLiteral *SL) {
    
    std::string Str = SL->getString().str();
    
    // Look for an existing one
    llvm::StringMap<llvm::Constant*>::iterator old = ObjCStrings.find(Str);
    if (old != ObjCStrings.end())
        return old->getValue();
    
    StringRef StringClass = CGM.getLangOpts().ObjCConstantStringClass;
    
    if (StringClass.empty()) StringClass = "NSConstantString";
    
    std::string Sym = "_NSConstantStringClassReference";
    //Sym += StringClass;
    
    llvm::Constant *isa = TheModule.getNamedGlobal(Sym);
    
    if (!isa)
        isa = new llvm::GlobalVariable(TheModule, IdTy, /* isConstant */false,
                                       llvm::GlobalValue::ExternalWeakLinkage, 0, Sym);
    else if (isa->getType() != PtrToIdTy)
        isa = llvm::ConstantExpr::getBitCast(isa, PtrToIdTy);
    
    std::vector<llvm::Constant*> Ivars;
    Ivars.push_back(isa);
    Ivars.push_back(MakeConstantString(Str));
    Ivars.push_back(llvm::ConstantInt::get(IntTy, Str.size()));
    llvm::Constant *ObjCStr = MakeGlobal(
                                         llvm::StructType::get(PtrToIdTy, PtrToInt8Ty, IntTy, NULL),
                                         Ivars, ".objc_str");
    ObjCStr = llvm::ConstantExpr::getBitCast(ObjCStr, PtrToInt8Ty);
    ObjCStrings[Str] = ObjCStr;
    ConstantStrings.push_back(ObjCStr);
    return ObjCStr;
}
#endif

/// OLD
/// Generate a class structure
llvm::Constant *CGObjCCocotron::GenerateClassStructureOld(
                                                  llvm::Constant *MetaClass,
                                                  llvm::Constant *SuperClass,
                                                  unsigned info,
                                                  const char *Name,
                                                  llvm::Constant *Version,
                                                  llvm::Constant *InstanceSize,
                                                  llvm::Constant *IVars,
                                                  llvm::Constant *Methods,
                                                  llvm::Constant *Protocols,
                                                  llvm::Constant *IvarOffsets,
                                                  llvm::Constant *Properties,
                                                  bool isMeta) {
    // Set up the class structure
    // Note:  Several of these are char*s when they should be ids.  This is
    // because the runtime performs this translation on load.
    //
    // Fields marked New ABI are part of the GNUstep runtime.  We emit them
    // anyway; the classes will still work with the GNU runtime, they will just
    // be ignored.
    llvm::StructType *ClassTy = llvm::StructType::get(PtrToInt8Ty,        // class_pointer
                                                      PtrToInt8Ty,        // super_class
                                                      PtrToInt8Ty,        // name
                                                      LongTy,             // version
                                                      LongTy,             // info
                                                      LongTy,             // instance_size
                                                      IVars->getType(),   // ivars
                                                      Methods->getType(), // methods
                                                      
                                                      // These are all filled in by the runtime, so we pretend
                                                      CachePtrTy,         // cache
                                                      PtrTy,              // protocols
                                                      PtrToInt8Ty,        // ivar layout
                                                      // New ABI:
                                                      LongTy,                 // abi_version
                                                      IvarOffsets->getType(), // ivar_offsets
                                                      Properties->getType(),  // properties
                                                      NULL);
    llvm::Constant *Zero = llvm::ConstantInt::get(LongTy, 0);
    // Fill in the structure
    std::vector<llvm::Constant*> Elements;
    Elements.push_back(llvm::ConstantExpr::getBitCast(MetaClass, PtrToInt8Ty));
    Elements.push_back(SuperClass);
    Elements.push_back(MakeConstantString(Name, ".class_name"));
    Elements.push_back(Zero);
    Elements.push_back(llvm::ConstantInt::get(LongTy, info));
    if (isMeta) {
        llvm::TargetData td(&TheModule);
        Elements.push_back(
                           llvm::ConstantInt::get(LongTy,
                                                  td.getTypeSizeInBits(ClassTy) /
                                                  CGM.getContext().getCharWidth()));
    } else
        Elements.push_back(InstanceSize);
    Elements.push_back(IVars);
    Elements.push_back(Methods);
    Elements.push_back(llvm::Constant::getNullValue(CachePtrTy)); //cache
    Elements.push_back(llvm::ConstantExpr::getBitCast(Protocols, PtrTy)); //protocol
    Elements.push_back(NULLPtr); //ivar layout
    Elements.push_back(Zero); //abi version
    Elements.push_back(IvarOffsets); //ivar_offset
    Elements.push_back(Properties); //propeties
    // Create an instance of the structure
    // This is now an externally visible symbol, so that we can speed up class
    // messages in the next ABI.  We may already have some weak references to
    // this, so check and fix them properly.
    std::string ClassSym((isMeta ? "_OBJC_METACLASS_": "_OBJC_CLASS_") +
                         std::string(Name));
    llvm::GlobalVariable *ClassRef = TheModule.getNamedGlobal(ClassSym);
    llvm::Constant *Class = MakeGlobal(ClassTy, Elements, ClassSym,
                                       llvm::GlobalValue::ExternalLinkage);
    if (ClassRef) {
        ClassRef->replaceAllUsesWith(llvm::ConstantExpr::getBitCast(Class,
                                                                    ClassRef->getType()));
        ClassRef->removeFromParent();
        Class->setName(ClassSym);
    }
    return Class;
}

///NEW
/// Generate a class structure
llvm::Constant *CGObjCCocotron::GenerateClassStructure(
                                                  llvm::Constant *MetaClass,
                                                  llvm::Constant *SuperClass,
                                                  unsigned info,
                                                  const char *Name,
                                                  llvm::Constant *Version,
                                                  llvm::Constant *InstanceSize,
                                                  llvm::Constant *IVars,
                                                  llvm::Constant *Methods,
                                                  llvm::Constant *Protocols,
                                                  llvm::Constant *IvarOffsets,
                                                  llvm::Constant *Properties,
                                                  llvm::Constant *StrongIvarBitmap,
                                                  llvm::Constant *WeakIvarBitmap,
                                                  bool isMeta) {
    // Set up the class structure
    // Note:  Several of these are char*s when they should be ids.  This is
    // because the runtime performs this translation on load.
    //
    // Fields marked New ABI are part of the GNUstep runtime.  We emit them
    // anyway; the classes will still work with the GNU runtime, they will just
    // be ignored.
    llvm::StructType *ClassTy = llvm::StructType::get(PtrToInt8Ty,        // isa
                                                      PtrToInt8Ty,        // super_class
                                                      PtrToInt8Ty,        // name
                                                      LongTy,             // version
                                                      LongTy,             // info
                                                      LongTy,             // instance_size
                                                      IVars->getType(),   // ivars
                                                      Methods->getType(), // methods
                                                      // These are all filled in by the runtime, so we pretend
                                                      CachePtrTy,         // dtable / cache
                                                      PtrTy,              // subclass_list
                                                      PtrTy,              // sibling_class
                                                      PtrTy,              // protocols
                                                      PtrTy,              // gc_object_type
                                                      // New ABI:
                                                      LongTy,                 // abi_version
                                                      IvarOffsets->getType(), // ivar_offsets
                                                      Properties->getType(),  // properties
                                                      IntPtrTy,               // strong_pointers
                                                      IntPtrTy,               // weak_pointers
                                                      NULL);
    
    llvm::Constant *Zero = llvm::ConstantInt::get(LongTy, 0);
    // Fill in the structure
    std::vector<llvm::Constant*> Elements;
    Elements.push_back(llvm::ConstantExpr::getBitCast(MetaClass, PtrToInt8Ty));
    Elements.push_back(SuperClass);
    Elements.push_back(MakeConstantString(Name, ".class_name"));
    Elements.push_back(Zero);
    Elements.push_back(llvm::ConstantInt::get(LongTy, info));
    if (isMeta) {
        llvm::TargetData td(&TheModule);
        Elements.push_back(
                           llvm::ConstantInt::get(LongTy,
                                                  td.getTypeSizeInBits(ClassTy) /
                                                  CGM.getContext().getCharWidth()));
    } else
        Elements.push_back(InstanceSize);
    
    Elements.push_back(IVars);
    Elements.push_back(Methods);
    Elements.push_back(llvm::Constant::getNullValue(CachePtrTy)); // dtable / cache
    Elements.push_back(NULLPtr);
    Elements.push_back(NULLPtr);
    Elements.push_back(llvm::ConstantExpr::getBitCast(Protocols, PtrTy));
    Elements.push_back(NULLPtr);
    Elements.push_back(llvm::ConstantInt::get(LongTy, 1));
    Elements.push_back(IvarOffsets);
    Elements.push_back(Properties);
    Elements.push_back(StrongIvarBitmap);
    Elements.push_back(WeakIvarBitmap);
    // Create an instance of the structure
    // This is now an externally visible symbol, so that we can speed up class
    // messages in the next ABI.  We may already have some weak references to
    // this, so check and fix them properly.
    std::string ClassSym((isMeta ? "_OBJC_METACLASS_": "_OBJC_CLASS_") +
                         std::string(Name));
    llvm::GlobalVariable *ClassRef = TheModule.getNamedGlobal(ClassSym);
    llvm::Constant *Class = MakeGlobal(ClassTy, Elements, ClassSym,
                                       llvm::GlobalValue::ExternalLinkage);
    if (ClassRef) {
        ClassRef->replaceAllUsesWith(llvm::ConstantExpr::getBitCast(Class,
                                                                    ClassRef->getType()));
        ClassRef->removeFromParent();
        Class->setName(ClassSym);
    }
    return Class;
}

llvm::Constant *CGObjCCocotron::GenerateEmptyProtocol(
                                                 const std::string &ProtocolName) {
    llvm::SmallVector<std::string, 0> EmptyStringVector;
    llvm::SmallVector<llvm::Constant*, 0> EmptyConstantVector;
    
    llvm::Constant *ProtocolList = GenerateProtocolList(EmptyStringVector);
    llvm::Constant *MethodList =
    GenerateProtocolMethodList(EmptyConstantVector, EmptyConstantVector);
    // Protocols are objects containing lists of the methods implemented and
    // protocols adopted.
    llvm::StructType *ProtocolTy = llvm::StructType::get(IdTy,
                                                         PtrToInt8Ty,
                                                         ProtocolList->getType(),
                                                         MethodList->getType(),
                                                         MethodList->getType(),
                                                         MethodList->getType(),
                                                         MethodList->getType(),
                                                         NULL);
    std::vector<llvm::Constant*> Elements;
    // The isa pointer must be set to a magic number so the runtime knows it's
    // the correct layout.
    Elements.push_back(llvm::ConstantExpr::getIntToPtr(
                                                       llvm::ConstantInt::get(llvm::Type::getInt32Ty(VMContext),
                                                                              ProtocolVersion), IdTy));
    Elements.push_back(MakeConstantString(ProtocolName, ".objc_protocol_name"));
    Elements.push_back(ProtocolList);
    Elements.push_back(MethodList);
    Elements.push_back(MethodList);
    Elements.push_back(MethodList);
    Elements.push_back(MethodList);
    return MakeGlobal(ProtocolTy, Elements, ".objc_protocol");
}

void CGObjCCocotron::GenerateClass(const ObjCImplementationDecl *OID) {
    ASTContext &Context = CGM.getContext();
    
    // Get the superclass name.
    const ObjCInterfaceDecl * SuperClassDecl =
    OID->getClassInterface()->getSuperClass();
    std::string SuperClassName;
    if (SuperClassDecl) {
        SuperClassName = SuperClassDecl->getNameAsString();
        EmitClassRef(SuperClassName);
    }
    
    // Get the class name
    ObjCInterfaceDecl *ClassDecl =
    const_cast<ObjCInterfaceDecl *>(OID->getClassInterface());
    std::string ClassName = ClassDecl->getNameAsString();
    // Emit the symbol that is used to generate linker errors if this class is
    // referenced in other modules but not declared.
    std::string classSymbolName = "__objc_class_name_" + ClassName;
    if (llvm::GlobalVariable *symbol =
        TheModule.getGlobalVariable(classSymbolName)) {
        symbol->setInitializer(llvm::ConstantInt::get(LongTy, 0));
    } else {
        new llvm::GlobalVariable(TheModule, LongTy, false,
                                 llvm::GlobalValue::ExternalLinkage, llvm::ConstantInt::get(LongTy, 0),
                                 classSymbolName);
    }
    
    // Get the size of instances.
    int instanceSize =
    Context.getASTObjCImplementationLayout(OID).getSize().getQuantity();
    
    // Collect information about instance variables.
    SmallVector<llvm::Constant*, 16> IvarNames;
    SmallVector<llvm::Constant*, 16> IvarTypes;
    SmallVector<llvm::Constant*, 16> IvarOffsets;
    
    std::vector<llvm::Constant*> IvarOffsetValues;
    SmallVector<bool, 16> WeakIvars;
    SmallVector<bool, 16> StrongIvars;
    
    int superInstanceSize = !SuperClassDecl ? 0 :
    Context.getASTObjCInterfaceLayout(SuperClassDecl).getSize().getQuantity();
    // For non-fragile ivars, set the instance size to 0 - {the size of just this
    // class}.  The runtime will then set this to the correct value on load.
    if (CGM.getContext().getLangOpts().ObjCNonFragileABI) {
        instanceSize = 0 - (instanceSize - superInstanceSize);
    }
    
    for (const ObjCIvarDecl *IVD = ClassDecl->all_declared_ivar_begin(); IVD;
         IVD = IVD->getNextIvar()) {
        // Store the name
        IvarNames.push_back(MakeConstantString(IVD->getNameAsString()));
        // Get the type encoding for this ivar
        std::string TypeStr;
        Context.getObjCEncodingForType(IVD->getType(), TypeStr);
        IvarTypes.push_back(MakeConstantString(TypeStr));
        // Get the offset
        uint64_t BaseOffset = ComputeIvarBaseOffset(CGM, OID, IVD);
        uint64_t Offset = BaseOffset;
        if (CGM.getContext().getLangOpts().ObjCNonFragileABI) {
            Offset = BaseOffset - superInstanceSize;
        }
        llvm::Constant *OffsetValue = llvm::ConstantInt::get(IntTy, Offset);
        // Create the direct offset value
        std::string OffsetName = "__objc_ivar_offset_value_" + ClassName +"." +
        IVD->getNameAsString();
        llvm::GlobalVariable *OffsetVar = TheModule.getGlobalVariable(OffsetName);
        if (OffsetVar) {
            OffsetVar->setInitializer(OffsetValue);
            // If this is the real definition, change its linkage type so that
            // different modules will use this one, rather than their private
            // copy.
            OffsetVar->setLinkage(llvm::GlobalValue::ExternalLinkage);
        } else
            OffsetVar = new llvm::GlobalVariable(TheModule, IntTy,
                                                 false, llvm::GlobalValue::ExternalLinkage,
                                                 OffsetValue,
                                                 "__objc_ivar_offset_value_" + ClassName +"." +
                                                 IVD->getNameAsString());
        IvarOffsets.push_back(OffsetValue);
        IvarOffsetValues.push_back(OffsetVar);
        Qualifiers::ObjCLifetime lt = IVD->getType().getQualifiers().getObjCLifetime();
        switch (lt) {
            case Qualifiers::OCL_Strong:
                StrongIvars.push_back(true);
                WeakIvars.push_back(false);
                break;
            case Qualifiers::OCL_Weak:
                StrongIvars.push_back(false);
                WeakIvars.push_back(true);
                break;
            default:
                StrongIvars.push_back(false);
                WeakIvars.push_back(false);
        }
    }
    llvm::Constant *StrongIvarBitmap = MakeBitField(StrongIvars);
    llvm::Constant *WeakIvarBitmap = MakeBitField(WeakIvars);
    llvm::GlobalVariable *IvarOffsetArray =
    MakeGlobalArray(PtrToIntTy, IvarOffsetValues, ".ivar.offsets");
    
    
    // Collect information about instance methods
    SmallVector<Selector, 16> InstanceMethodSels;
    SmallVector<llvm::Constant*, 16> InstanceMethodTypes;
    for (ObjCImplementationDecl::instmeth_iterator
         iter = OID->instmeth_begin(), endIter = OID->instmeth_end();
         iter != endIter ; iter++) {
        InstanceMethodSels.push_back((*iter)->getSelector());
        std::string TypeStr;
        Context.getObjCEncodingForMethodDecl((*iter),TypeStr);
        InstanceMethodTypes.push_back(MakeConstantString(TypeStr));
    }
    
    llvm::Constant *Properties = GeneratePropertyList(OID, InstanceMethodSels,
                                                      InstanceMethodTypes);
    
    
    // Collect information about class methods
    SmallVector<Selector, 16> ClassMethodSels;
    SmallVector<llvm::Constant*, 16> ClassMethodTypes;
    for (ObjCImplementationDecl::classmeth_iterator
         iter = OID->classmeth_begin(), endIter = OID->classmeth_end();
         iter != endIter ; iter++) {
        ClassMethodSels.push_back((*iter)->getSelector());
        std::string TypeStr;
        Context.getObjCEncodingForMethodDecl((*iter),TypeStr);
        ClassMethodTypes.push_back(MakeConstantString(TypeStr));
    }
    // Collect the names of referenced protocols
    SmallVector<std::string, 16> Protocols;
    for (ObjCInterfaceDecl::protocol_iterator
         I = ClassDecl->protocol_begin(),
         E = ClassDecl->protocol_end(); I != E; ++I)
        Protocols.push_back((*I)->getNameAsString());
    
    
    
    // Get the superclass pointer.
    llvm::Constant *SuperClass;
    if (!SuperClassName.empty()) {
        SuperClass = MakeConstantString(SuperClassName, ".super_class_name");
    } else {
        SuperClass = llvm::ConstantPointerNull::get(PtrToInt8Ty);
    }
    // Empty vector used to construct empty method lists
    SmallVector<llvm::Constant*, 1>  empty;
    // Generate the method and instance variable lists
    llvm::Constant *MethodList = GenerateMethodList(ClassName, "",
                                                    InstanceMethodSels, InstanceMethodTypes, false);
    llvm::Constant *ClassMethodList = GenerateMethodList(ClassName, "",
                                                         ClassMethodSels, ClassMethodTypes, true);
    llvm::Constant *IvarList = GenerateIvarList(IvarNames, IvarTypes,
                                                IvarOffsets);
    // Irrespective of whether we are compiling for a fragile or non-fragile ABI,
    // we emit a symbol containing the offset for each ivar in the class.  This
    // allows code compiled for the non-Fragile ABI to inherit from code compiled
    // for the legacy ABI, without causing problems.  The converse is also
    // possible, but causes all ivar accesses to be fragile.
    
    // Offset pointer for getting at the correct field in the ivar list when
    // setting up the alias.  These are: The base address for the global, the
    // ivar array (second field), the ivar in this list (set for each ivar), and
    // the offset (third field in ivar structure)
    llvm::Type *IndexTy = Int32Ty;
    llvm::Constant *offsetPointerIndexes[] = {Zeros[0],
        llvm::ConstantInt::get(IndexTy, 1), 0,
        llvm::ConstantInt::get(IndexTy, 2) };
    
    unsigned ivarIndex = 0;
    for (const ObjCIvarDecl *IVD = ClassDecl->all_declared_ivar_begin(); IVD;
         IVD = IVD->getNextIvar()) {
        const std::string Name = "__objc_ivar_offset_" + ClassName + '.'
        + IVD->getNameAsString();
        offsetPointerIndexes[2] = llvm::ConstantInt::get(IndexTy, ivarIndex);
        // Get the correct ivar field
        llvm::Constant *offsetValue = llvm::ConstantExpr::getGetElementPtr(
                                                                           IvarList, offsetPointerIndexes);
        // Get the existing variable, if one exists.
        llvm::GlobalVariable *offset = TheModule.getNamedGlobal(Name);
        if (offset) {
            offset->setInitializer(offsetValue);
            // If this is the real definition, change its linkage type so that
            // different modules will use this one, rather than their private
            // copy.
            offset->setLinkage(llvm::GlobalValue::ExternalLinkage);
        } else {
            // Add a new alias if there isn't one already.
            offset = new llvm::GlobalVariable(TheModule, offsetValue->getType(),
                                              false, llvm::GlobalValue::ExternalLinkage, offsetValue, Name);
            (void) offset; // Silence dead store warning.
        }
        ++ivarIndex;
    }
    llvm::Constant *ZeroPtr = llvm::ConstantInt::get(IntPtrTy, 0);
    //Generate metaclass for class methods
#if 0
    llvm::Constant *MetaClassStruct = GenerateClassStructureOld(NULLPtr,
                                                                NULLPtr, 0x12L, ClassName.c_str(), 0, Zeros[0], GenerateIvarList(
                                                                                                                                 empty, empty, empty), ClassMethodList, NULLPtr, NULLPtr, NULLPtr, true);
#else
    //New:
    llvm::Constant *MetaClassStruct = GenerateClassStructure(NULLPtr,
                                                             NULLPtr, 0x12L, ClassName.c_str(), 0, Zeros[0], GenerateIvarList(
                                                                                                                              empty, empty, empty), ClassMethodList, NULLPtr,
                                                             NULLPtr, NULLPtr, ZeroPtr, ZeroPtr, true);
#endif
    
    // Generate the class structure
#if 0
    llvm::Constant *ClassStruct =
    GenerateClassStructureOld(MetaClassStruct, SuperClass, 0x11L,
                           ClassName.c_str(), 0,
                           llvm::ConstantInt::get(LongTy, instanceSize), IvarList,
                           MethodList, GenerateProtocolList(Protocols), IvarOffsetArray,
                           Properties);
#else
    // NEW: CHECK FIXME
    // Generate the class structure
    llvm::Constant *ClassStruct =
    GenerateClassStructure(MetaClassStruct, SuperClass, 0x11L,
                           ClassName.c_str(), 0,
                           llvm::ConstantInt::get(LongTy, instanceSize), IvarList,
                           MethodList, GenerateProtocolList(Protocols), IvarOffsetArray,
                           Properties, StrongIvarBitmap, WeakIvarBitmap);
#endif
    
    // Resolve the class aliases, if they exist.
    if (ClassPtrAlias) {
        ClassPtrAlias->replaceAllUsesWith(
                                          llvm::ConstantExpr::getBitCast(ClassStruct, IdTy));
        ClassPtrAlias->eraseFromParent();
        ClassPtrAlias = 0;
    }
    if (MetaClassPtrAlias) {
        MetaClassPtrAlias->replaceAllUsesWith(
                                              llvm::ConstantExpr::getBitCast(MetaClassStruct, IdTy));
        MetaClassPtrAlias->eraseFromParent();
        MetaClassPtrAlias = 0;
    }
    
    // Add class structure to list to be added to the symtab later
    ClassStruct = llvm::ConstantExpr::getBitCast(ClassStruct, PtrToInt8Ty);
    Classes.push_back(ClassStruct);
}

/*
llvm::Function *CGObjCCocotron::ModuleInitFunction() {
    // Only emit an ObjC load function if no Objective-C stuff has been called
    if (Classes.empty() && Categories.empty() && ConstantStrings.empty() &&
        ExistingProtocols.empty() && SelectorTable.empty())
        return NULL;
    
    // Add all referenced protocols to a category.
    GenerateProtocolHolderCategory();
    
    // Name the ObjC types to make the IR a bit easier to read
    TheModule.addTypeName(".objc_selector", PtrToInt8Ty);
    TheModule.addTypeName(".objc_id", IdTy);
    TheModule.addTypeName(".objc_imp", IMPTy);
 
 .
 .
 .
 */


llvm::Value *CGObjCCocotron::EmitIvarOffset(CodeGenFunction &CGF,
                                       const ObjCInterfaceDecl *Interface,
                                       const ObjCIvarDecl *Ivar) {
    if (CGM.getLangOpts().ObjCNonFragileABI) {
        Interface = FindIvarInterface(CGM.getContext(), Interface, Ivar);
        return CGF.Builder.CreateZExtOrBitCast(
                                               CGF.Builder.CreateLoad(CGF.Builder.CreateLoad(
                                                                                             ObjCIvarOffsetVariable(Interface, Ivar), false, "ivar")),
                                               PtrDiffTy);
    }
    uint64_t Offset = ComputeIvarBaseOffset(CGF.CGM, Interface, Ivar);
    return llvm::ConstantInt::get(PtrDiffTy, Offset, "ivar");
}

CGObjCRuntime *
clang::CodeGen::CreateCocotronObjCRuntime(CodeGenModule &CGM) {
    return new CGObjCCocotron(CGM);
}

#endif
#endif
