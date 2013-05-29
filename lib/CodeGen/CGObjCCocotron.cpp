//===---- CGObjCCocotron.cpp - Emit LLVM Code from ASTs for a Module ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides Objective-C code generation targeting the Cocotron runtime.
// The class in this file generates structures used by the Cocotron Objective-C
// runtime library.  These structures are defined in objc/objc.h and
// objc/objc-api.h in the Cocotron runtime distribution.
//
//===----------------------------------------------------------------------===//

//using llvm::dyn_cast;

#ifdef __INCLUDED_FROM_ANOTHER_MODULE__
#ifdef __RUNTIME_CLASS_DECLARATION__
/* Note: Blank space intentional in this file to ease diffing with CGObjCGNU */






















/// Cocotron Objective-C runtime code generation.  This class implements the parts of
/// Objective-C support that are specific to the Cocotron stock runtime
class CGObjCCocotron : public CGObjCGNU {
    /// The GCC ABI message lookup function.  Returns an IMP pointing to the
    /// method implementation for this message.
    LazyRuntimeFunction MsgLookupFn;
    /// The GCC ABI superclass message lookup function.  Takes a pointer to a
    /// structure describing the receiver and the class, and a selector as
    /// arguments.  Returns the IMP for the corresponding method.
    LazyRuntimeFunction MsgLookupSuperFn;
    
    /// CacheTy - LLVM type for struct objc_cache.
    llvm::Type *CacheTy;
    /// CachePtrTy - LLVM type for struct objc_cache *.
    llvm::Type *CachePtrTy;
    
protected:
    /// Looks up the method for sending a message to the specified object.
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
    /// Looks up the method for sending a message to a superclass.
    virtual llvm::Value *LookupIMPSuper(CodeGenFunction &CGF,
                                      llvm::Value *ObjCSuper,
                                      llvm::Value *cmd) {
      CGBuilderTy &Builder = CGF.Builder;
      llvm::Value *lookupArgs[] = {EnforceType(Builder, ObjCSuper,
          PtrToObjCSuperTy), cmd};
      return Builder.CreateCall(MsgLookupSuperFn, lookupArgs);
    }
    
protected:
    /// Generates a class structure.
    virtual llvm::Constant *GenerateClassStructure(
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
    
    virtual llvm::Value *GetSelector(CGBuilderTy &Builder, Selector Sel,
                                     const std::string &TypeEncoding, bool lval);
    
public:
    CGObjCCocotron(CodeGenModule &Mod) : CGObjCGNU(Mod, 0, 0) {
        // IMP objc_msg_lookup(id, SEL);
        MsgLookupFn.init(&CGM, "objc_msg_lookup", IMPTy, IdTy, SelectorTy, NULL);
        // IMP objc_msg_lookup_super(struct objc_super*, SEL);
        MsgLookupSuperFn.init(&CGM, "objc_msg_lookup_super", IMPTy,
                              PtrToObjCSuperTy, SelectorTy, NULL);
                              
        //No support in runtime for NonFragileABI ATM:
        f_runtime_supports_nfabi = false;

    		CacheTy = llvm::StructType::get(VMContext);
        CGM.getModule().getOrInsertGlobal("struct._objc_cache", CacheTy);
        CachePtrTy = llvm::PointerType::getUnqual(CacheTy);
        
        // Force the selector Type.
        SelectorTy = PtrToInt8Ty;
        
        //Force the runtime
        //If using ARC the super constructor will change this to 10 and
        //do bad things (i.e. ivar offsets that are not currently supported
        //by the cocotron runtime)
        RuntimeVersion = 0; 
    }
    
    //these two are the exact same as the base class.. remove?
    virtual llvm::Value *GetSelector(CGBuilderTy &Builder, Selector Sel,
                                     bool lval = false);
    virtual llvm::Value *GetSelector(CGBuilderTy &Builder, const ObjCMethodDecl
                                     *Method);
    
    virtual llvm::Constant *GenerateConstantString(const StringLiteral *);
    virtual llvm::Function *ModuleInitFunction();
};

#else // Class Implementation




































llvm::Value *CGObjCCocotron::GetSelector(CGBuilderTy &Builder, Selector Sel,
                                         const std::string &TypeEncoding, bool lval) {
    
    /* TODO FIX IT
     llvm::GlobalAlias *&SelValue =  SelectorTable[Sel];
     
     if (0 == SelValue) {
     SelValue = new llvm::GlobalAlias(PtrToInt8Ty,
     llvm::GlobalValue::PrivateLinkage,
     ".objc_selector_alias_"+Sel.getAsString(), NULL,
     &TheModule);
     
     }
     
     if (lval) {
     llvm::Value *tmp = Builder.CreateAlloca(SelValue->getType());
     Builder.CreateStore(SelValue, tmp);
     return tmp;
     }
     
     return SelValue;
     .
     .
     .
     original:
     llvm::SmallVector<TypedSelector, 2> &Types = SelectorTable[Sel];
     llvm::GlobalAlias *SelValue = 0;
     
     for (llvm::SmallVectorImpl<TypedSelector>::iterator i = Types.begin(),
     e = Types.end() ; i!=e ; i++) {
     if (i->first == TypeEncoding) {
     SelValue = i->second;
     break;
     }
     }
     if (0 == SelValue) {
     SelValue = new llvm::GlobalAlias(SelectorTy,
     llvm::GlobalValue::PrivateLinkage,
     ".objc_selector_"+Sel.getAsString(), NULL,
     &TheModule);
     Types.push_back(TypedSelector(TypeEncoding, SelValue));
     }
     
     if (lval) {
     llvm::Value *tmp = Builder.CreateAlloca(SelValue->getType());
     Builder.CreateStore(SelValue, tmp);
     return tmp;
     }
     return SelValue;
     */
    
    llvm::Value *selectorString = CGM.GetAddrOfConstantCString(Sel.getAsString());
    selectorString = Builder.CreateStructGEP(selectorString, 0);
    
    llvm::Constant *sel_getUidFn =
    CGM.CreateRuntimeFunction(llvm::FunctionType::get(IdTy,
                                                      PtrToInt8Ty,
                                                      true),
                              "sel_getUid");
    
    return Builder.CreateCall(sel_getUidFn, selectorString);
}

//these two are the exact same as the base class.. remove? IF not, keep exactly like base's
llvm::Value *CGObjCCocotron::GetSelector(CGBuilderTy &Builder, Selector Sel,
                                         bool lval) {
    return GetSelector(Builder, Sel, std::string(), lval);
}

llvm::Value *CGObjCCocotron::GetSelector(CGBuilderTy &Builder, const ObjCMethodDecl
                                         *Method) {
    std::string SelTypes;
    CGM.getContext().getObjCEncodingForMethodDecl(Method, SelTypes);
    return GetSelector(Builder, Method->getSelector(), SelTypes, false);
}



/// Generate an NSConstantString object.
llvm::Constant *CGObjCCocotron::GenerateConstantString(const StringLiteral *SL) {
    std::string Str = SL->getString().str();
    
    // Look for an existing one
    llvm::StringMap<llvm::Constant*>::iterator old = ObjCStrings.find(Str);
    if (old != ObjCStrings.end())
        return old->getValue();
    
    std::vector<llvm::Constant*> Ivars;
    //Ivars.push_back(NULLPtr); //Cocotron Commented out
    Ivars.push_back(MakeConstantString("_NSConstantStringClassReference")); //Cocotron specific (instead of pushback MakeConstantString
    Ivars.push_back(MakeConstantString(Str));
    Ivars.push_back(llvm::ConstantInt::get(IntTy, Str.size()));
    llvm::Constant *ObjCStr = MakeGlobal(
                                         llvm::StructType::get(PtrToIntTy, PtrToInt8Ty, IntTy, NULL), //PtrToIntTy is Cocotron's chg
                                         Ivars, ".objc_str");
    ObjCStr = llvm::ConstantExpr::getBitCast(ObjCStr, PtrToInt8Ty);
    ObjCStrings[Str] = ObjCStr;
    ConstantStrings.push_back(ObjCStr);
    return ObjCStr;
}









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
    
    //Start Cocotron
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
    //End Cocotron
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
    //Start Cocotron
    Elements.push_back(llvm::Constant::getNullValue(CachePtrTy)); //cache
    Elements.push_back(llvm::ConstantExpr::getBitCast(Protocols, PtrTy)); //protocol
    Elements.push_back(NULLPtr); //ivar layout
    Elements.push_back(Zero); //abi version
    Elements.push_back(IvarOffsets); //ivar_offset
    Elements.push_back(Properties); //propeties
    //End Cocotron (don't include more elements above)
    // Create an instance of the structure
    // This is now an externally visible symbol, so that we can speed up class
    // messages in the next ABI.
    return MakeGlobal(ClassTy, Elements, (isMeta ? "_OBJC_METACLASS_":
                                          "_OBJC_CLASS_") + std::string(Name), llvm::GlobalValue::ExternalLinkage);
}












































llvm::Function *CGObjCCocotron::ModuleInitFunction() {
    // Only emit an ObjC load function if no Objective-C stuff has been called
    if (Classes.empty() && Categories.empty() && ConstantStrings.empty() &&
        ExistingProtocols.empty() && SelectorTable.empty())
        return NULL;
    
    // Add all referenced protocols to a category.
    GenerateProtocolHolderCategory();
    
    /* cocotron disabled
     llvm::StructType *SelStructTy = dyn_cast<llvm::StructType>(
     SelectorTy->getElementType());
     llvm::Type *SelStructPtrTy = SelectorTy;
     if (SelStructTy == 0) {
     SelStructTy = llvm::StructType::get(PtrToInt8Ty, PtrToInt8Ty, NULL);
     SelStructPtrTy = llvm::PointerType::getUnqual(SelStructTy);
     }
     */
    
    std::vector<llvm::Constant*> Elements;
    llvm::Constant *Statics = NULLPtr;
    // Generate statics list:
    if (ConstantStrings.size()) {
        llvm::ArrayType *StaticsArrayTy = llvm::ArrayType::get(PtrToInt8Ty,
                                                               ConstantStrings.size() + 1);
        ConstantStrings.push_back(NULLPtr);
        
        StringRef StringClass = CGM.getLangOptions().ObjCConstantStringClass;
        
        StringClass = "NSConstantString"; //cocotron no if stringclass empty
        Elements.push_back(MakeConstantString(StringClass,
                                              ".objc_static_class_name"));
        Elements.push_back(llvm::ConstantArray::get(StaticsArrayTy,
                                                    ConstantStrings));
        llvm::StructType *StaticsListTy =
        llvm::StructType::get(PtrToInt8Ty, StaticsArrayTy, NULL);
        llvm::Type *StaticsListPtrTy =
        llvm::PointerType::getUnqual(StaticsListTy);
        Statics = MakeGlobal(StaticsListTy, Elements, ".objc_statics");
        llvm::ArrayType *StaticsListArrayTy =
        llvm::ArrayType::get(StaticsListPtrTy, 2);
        Elements.clear();
        Elements.push_back(Statics);
        Elements.push_back(llvm::Constant::getNullValue(StaticsListPtrTy));
        Statics = MakeGlobal(StaticsListArrayTy, Elements, ".objc_statics_ptr");
        Statics = llvm::ConstantExpr::getBitCast(Statics, PtrTy);
    }
    
    // Array of classes, categories, and constant objects
    llvm::ArrayType *ClassListTy = llvm::ArrayType::get(PtrToInt8Ty,
                                                        Classes.size() + Categories.size()  + 2);
    llvm::StructType *SymTabTy = llvm::StructType::get(LongTy, PtrToInt8Ty, //Cocotron PtrToInt8Ty
                                                       llvm::Type::getInt16Ty(VMContext),
                                                       llvm::Type::getInt16Ty(VMContext),
                                                       ClassListTy, NULL);
    
    Elements.clear();
    // Pointer to an array of selectors used in this module.
    std::vector<llvm::Constant*> Selectors;
    std::vector<llvm::GlobalAlias*> SelectorAliases;
    for (SelectorMap::iterator iter = SelectorTable.begin(),
         iterEnd = SelectorTable.end(); iter != iterEnd ; ++iter) {
        
        std::string SelNameStr = iter->first.getAsString();
        llvm::Constant *SelName = ExportUniqueString(SelNameStr, ".objc_sel_name");
        Selectors.push_back(SelName); //Cocotron like this
        /*
         SmallVectorImpl<TypedSelector> &Types = iter->second;
         for (SmallVectorImpl<TypedSelector>::iterator i = Types.begin(),
         e = Types.end() ; i!=e ; i++) {
         
         llvm::Constant *SelectorTypeEncoding = NULLPtr;
         if (!i->first.empty())
         SelectorTypeEncoding = MakeConstantString(i->first, ".objc_sel_types");
         
         Elements.push_back(SelName);
         Elements.push_back(SelectorTypeEncoding);
         Selectors.push_back(llvm::ConstantStruct::get(SelStructTy, Elements));
         Elements.clear();
         
         // Store the selector alias for later replacement
         SelectorAliases.push_back(i->second);
         }
         */
        //SelectorAliases.push_back(iter->second);
    }
    
    unsigned SelectorCount = Selectors.size();
    // NULL-terminate the selector list. Cocotron needs this in the current implementation
    Selectors.push_back(NULLPtr);
    Elements.clear();
    
    
    // Number of static selectors
    Elements.push_back(llvm::ConstantInt::get(LongTy, SelectorCount));
    
    
    llvm::Constant *SelectorList = MakeGlobalArray(PtrToInt8Ty, Selectors, //Cocotron PtrToInt8Ty
                                                   ".objc_selector_list");
    
    Elements.push_back(llvm::ConstantExpr::getBitCast(SelectorList,
                                                      PtrToInt8Ty)); //Cocotron PtrToInt8Ty
    
    // Now that all of the static selectors exist, create pointers to them.
    for (unsigned int i=0 ; i<SelectorCount ; i++) {
        llvm::Constant *Idxs[] = {Zeros[0],
            llvm::ConstantInt::get(Int32Ty, i), Zeros[0]};
        
        // FIXME: We're generating redundant loads and stores here!
        llvm::Constant *SelPtr = llvm::ConstantExpr::getGetElementPtr(SelectorList,
                                                                     makeArrayRef(Idxs, 2));
        
        // If selectors are defined as an opaque type, cast the pointer to this
        // type.
        SelPtr = llvm::ConstantExpr::getBitCast(SelPtr, PtrToInt8Ty); //Cocotron PtrToInt8Ty
        
        SelectorAliases[i]->replaceAllUsesWith(SelPtr);
        
        SelectorAliases[i]->eraseFromParent();
    }
    
    // Number of classes defined.
    Elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt16Ty(VMContext),
                                              Classes.size()));
    // Number of categories defined
    Elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt16Ty(VMContext),
                                              Categories.size()));
    // Create an array of classes, then categories, then static object instances
    Classes.insert(Classes.end(), Categories.begin(), Categories.end());
    //  NULL-terminated list of static object instances (mainly constant strings)
    Classes.push_back(Statics);
    Classes.push_back(NULLPtr);
    llvm::Constant *ClassList = llvm::ConstantArray::get(ClassListTy, Classes);
    Elements.push_back(ClassList);
    // Construct the symbol table
    llvm::Constant *SymTab= MakeGlobal(SymTabTy, Elements);
    
    // The symbol table is contained in a module which has some version-checking
    // constants
    llvm::StructType * ModuleTy = llvm::StructType::get(LongTy, LongTy,
	PtrToInt8Ty, llvm::PointerType::getUnqual(SymTabTy),
	(RuntimeVersion >= 10) ? IntTy : NULL, NULL);
    Elements.clear();
    // Runtime version, used for ABI compatibility checking.
    Elements.push_back(llvm::ConstantInt::get(LongTy, RuntimeVersion));
    // sizeof(ModuleTy)
    llvm::TargetData td(&TheModule);
    Elements.push_back(
                       llvm::ConstantInt::get(LongTy,
                                              td.getTypeSizeInBits(ModuleTy) /
                                              CGM.getContext().getCharWidth()));
    
    // The path to the source file where this module was declared
    SourceManager &SM = CGM.getContext().getSourceManager();
    const FileEntry *mainFile = SM.getFileEntryForID(SM.getMainFileID());
    std::string path =
    std::string(mainFile->getDir()->getName()) + '/' + mainFile->getName();
    Elements.push_back(MakeConstantString(path, ".objc_source_file_name"));
    Elements.push_back(SymTab);
	
    if (RuntimeVersion >= 10)
    switch (CGM.getLangOptions().getGC()) {
      case LangOptions::GCOnly:
        Elements.push_back(llvm::ConstantInt::get(IntTy, 2));
        break;
      case LangOptions::NonGC:
        if (CGM.getLangOptions().ObjCAutoRefCount)
          Elements.push_back(llvm::ConstantInt::get(IntTy, 1));
        else
          Elements.push_back(llvm::ConstantInt::get(IntTy, 0));
        break;
      case LangOptions::HybridGC:
          Elements.push_back(llvm::ConstantInt::get(IntTy, 1));
        break;
    }
	
    llvm::Value *Module = MakeGlobal(ModuleTy, Elements);
    
    // Create the load function calling the runtime entry point with the module
    // structure
    llvm::Function * LoadFunction = llvm::Function::Create(
                                                           llvm::FunctionType::get(llvm::Type::getVoidTy(VMContext), false),
                                                           llvm::GlobalValue::InternalLinkage, ".objc_load_function",
                                                           &TheModule);
    llvm::BasicBlock *EntryBB =
    llvm::BasicBlock::Create(VMContext, "entry", LoadFunction);
    CGBuilderTy Builder(VMContext);
    Builder.SetInsertPoint(EntryBB);
    
	llvm::FunctionType *FT =
    llvm::FunctionType::get(Builder.getVoidTy(),
                            llvm::PointerType::getUnqual(ModuleTy), true);
	llvm::Value *Register = CGM.CreateRuntimeFunction(FT, "__objc_exec_class");
    Builder.CreateCall(Register, Module);
    Builder.CreateRetVoid();
    
    return LoadFunction;
}


























CGObjCRuntime *
clang::CodeGen::CreateCocotronObjCRuntime(CodeGenModule &CGM) {
    return new CGObjCCocotron(CGM);
}

#endif
#endif
