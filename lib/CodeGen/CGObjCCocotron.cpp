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

/// Cocotron Objective-C runtime code generation.  This class implements the parts of
/// Objective-C support that are specific to the Cocotron stock runtime
class CGObjCCocotron : public CGObjCGNU {
private:
		/// CacheTy - LLVM type for struct objc_cache.
		const llvm::Type *CacheTy;
		/// CachePtrTy - LLVM type for struct objc_cache *.
		const llvm::Type *CachePtrTy;
		
private:
		/// The GCC ABI message lookup function.  Returns an IMP pointing to the
		/// method implementation for this message.
		LazyRuntimeFunction MsgLookupFn;
		/// The GCC ABI superclass message lookup function.  Takes a pointer to a
		/// structure describing the receiver and the class, and a selector as
		/// arguments.  Returns the IMP for the corresponding method.
		LazyRuntimeFunction MsgLookupSuperFn;
		

protected:
		/// Looks up the method for sending a message to the specified object.
		virtual llvm::Value *LookupIMP(CodeGenFunction &CGF,
															 llvm::Value *&Receiver,
															 llvm::Value *cmd,
															 llvm::MDNode *node) {
				CGBuilderTy &Builder = CGF.Builder;
				llvm::Value *imp = Builder.CreateCall2(MsgLookupFn, 
							EnforceType(Builder, Receiver, IdTy),
							EnforceType(Builder, cmd, SelectorTy));
				cast<llvm::CallInst>(imp)->setMetadata(msgSendMDKind, node);
				return imp;
		}
		/// Looks up the method for sending a message to a superclass.
		virtual llvm::Value *LookupIMPSuper(CodeGenFunction &CGF,
                                      llvm::Value *ObjCSuper,
                                      llvm::Value *cmd) {
				CGBuilderTy &Builder = CGF.Builder;
				llvm::Value *lookupArgs[] = {EnforceType(Builder, ObjCSuper,
						PtrToObjCSuperTy), cmd};
				return Builder.CreateCall(MsgLookupSuperFn, lookupArgs, lookupArgs+2);
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
				bool isMeta=false);

		virtual llvm::Value *GetSelector(CGBuilderTy &Builder, Selector Sel,
			const std::string &TypeEncoding, bool lval);

public:
		virtual llvm::Value *GetClass(CGBuilderTy &Builder,
														const ObjCInterfaceDecl *OID);

		    
public:
		CGObjCCocotron(CodeGenModule &Mod) : CGObjCGNU(Mod, 0, 0) {
			// IMP objc_msg_lookup(id, SEL);
			MsgLookupFn.init(&CGM, "objc_msg_lookup", IMPTy, IdTy, SelectorTy, NULL);
			// IMP objc_msg_lookup_super(struct objc_super*, SEL);
			MsgLookupSuperFn.init(&CGM, "objc_msg_lookup_super", IMPTy,
								PtrToObjCSuperTy, SelectorTy, NULL);
					
			CacheTy = llvm::OpaqueType::get(VMContext);
			CGM.getModule().addTypeName("struct._objc_cache", CacheTy);
			CachePtrTy = llvm::PointerType::getUnqual(CacheTy);

			// Force the selector Type.
			SelectorTy = PtrToInt8Ty;
		}

		//these two are the exact same as the base class.. remove?		
		virtual llvm::Value *GetSelector(CGBuilderTy &Builder, Selector Sel,
																		 bool lval = false);
		virtual llvm::Value *GetSelector(CGBuilderTy &Builder, const ObjCMethodDecl
																		*Method);

		virtual llvm::Constant *GenerateConstantString(const StringLiteral *);
		virtual void GenerateClass(const ObjCImplementationDecl *ClassDecl);
		virtual llvm::Function *ModuleInitFunction();
		virtual llvm::Value *EmitIvarOffset(CodeGenFunction &CGF,
																				const ObjCInterfaceDecl *Interface,
																				const ObjCIvarDecl *Ivar);

};

#else // Class Implementation

// This has to perform the lookup every time, since posing and related
// techniques can modify the name -> class mapping.
llvm::Value *CGObjCCocotron::GetClass(CGBuilderTy &Builder,
                                 const ObjCInterfaceDecl *OID) {
    llvm::Value *ClassName = CGM.GetAddrOfConstantCString(OID->getNameAsString());
    // With the incompatible ABI, this will need to be replaced with a direct
    // reference to the class symbol.  For the compatible nonfragile ABI we are
    // still performing this lookup at run time but emitting the symbol for the
    // class externally so that we can make the switch later.
    EmitClassRef(OID->getNameAsString());
    ClassName = Builder.CreateStructGEP(ClassName, 0);
    
    llvm::Constant *ClassLookupFn =
	CGM.CreateRuntimeFunction(llvm::FunctionType::get(IdTy, PtrToInt8Ty, true),
                                                      "objc_lookUpClass");
    return Builder.CreateCall(ClassLookupFn, ClassName);
}

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
    
    std::vector<const llvm::Type*> Params(1, PtrToInt8Ty);
    llvm::Constant *sel_getUidFn =
    CGM.CreateRuntimeFunction(llvm::FunctionType::get(IdTy,
                                                      Params,
                                                      true),
                                                "sel_getUid");
    
    return Builder.CreateCall(sel_getUidFn, selectorString);
}

//these two are the exact same as the base class.. remove?
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
    
    llvm::Constant *Zero =
    llvm::Constant::getNullValue(llvm::Type::getInt32Ty(VMContext));
    llvm::Constant *Zeros[] = { Zero, Zero };
    
    const llvm::Type *Ty = CGM.getTypes().ConvertType(CGM.getContext().IntTy);
    Ty = llvm::ArrayType::get(Ty, 0);
    
    llvm::Constant *GV;
    GV = CGM.CreateRuntimeVariable(Ty, "_NSConstantStringClassReference");
    
    llvm::Constant *ConstantStringClassRef = llvm::ConstantExpr::getGetElementPtr(GV, Zeros, 2);    
    
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
                                                      CachePtrTy,              // cache
                                                      PtrTy,              // protocols
                                                      PtrToInt8Ty,              // ivar layout
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
    // messages in the next ABI.
    return MakeGlobal(ClassTy, Elements, (isMeta ? "_OBJC_METACLASS_":
                                          "_OBJC_CLASS_") + std::string(Name), llvm::GlobalValue::ExternalLinkage);
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
    llvm::SmallVector<llvm::Constant*, 16> IvarNames;
    llvm::SmallVector<llvm::Constant*, 16> IvarTypes;
    llvm::SmallVector<llvm::Constant*, 16> IvarOffsets;
    
    std::vector<llvm::Constant*> IvarOffsetValues;
    
    int superInstanceSize = !SuperClassDecl ? 0 :
    Context.getASTObjCInterfaceLayout(SuperClassDecl).getSize().getQuantity();
    // For non-fragile ivars, set the instance size to 0 - {the size of just this
    // class}.  The runtime will then set this to the correct value on load.
    if (CGM.getContext().getLangOptions().ObjCNonFragileABI && !true) {
        instanceSize = 0 - (instanceSize - superInstanceSize);
    }
    
    // Collect declared and synthesized ivars.
    llvm::SmallVector<ObjCIvarDecl*, 16> OIvars;
    CGM.getContext().ShallowCollectObjCIvars(ClassDecl, OIvars);
    
    for (unsigned i = 0, e = OIvars.size(); i != e; ++i) {
        ObjCIvarDecl *IVD = OIvars[i];
        // Store the name
        IvarNames.push_back(MakeConstantString(IVD->getNameAsString()));
        // Get the type encoding for this ivar
        std::string TypeStr;
        Context.getObjCEncodingForType(IVD->getType(), TypeStr);
        IvarTypes.push_back(MakeConstantString(TypeStr));
        // Get the offset
        uint64_t BaseOffset = ComputeIvarBaseOffset(CGM, OID, IVD);
        uint64_t Offset = BaseOffset;
        if (CGM.getContext().getLangOptions().ObjCNonFragileABI && !true) {
            Offset = BaseOffset - superInstanceSize;
        }
        IvarOffsets.push_back(
                              llvm::ConstantInt::get(llvm::Type::getInt32Ty(VMContext), Offset));
        IvarOffsetValues.push_back(new llvm::GlobalVariable(TheModule, IntTy,
                                                            false, llvm::GlobalValue::ExternalLinkage,
                                                            llvm::ConstantInt::get(IntTy, Offset),
                                                            "__objc_ivar_offset_value_" + ClassName +"." +
                                                            IVD->getNameAsString()));
    }
    llvm::GlobalVariable *IvarOffsetArray =
    MakeGlobalArray(PtrToIntTy, IvarOffsetValues, ".ivar.offsets");
    
    
    // Collect information about instance methods
    llvm::SmallVector<Selector, 16> InstanceMethodSels;
    llvm::SmallVector<llvm::Constant*, 16> InstanceMethodTypes;
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
    llvm::SmallVector<Selector, 16> ClassMethodSels;
    llvm::SmallVector<llvm::Constant*, 16> ClassMethodTypes;
    for (ObjCImplementationDecl::classmeth_iterator
         iter = OID->classmeth_begin(), endIter = OID->classmeth_end();
         iter != endIter ; iter++) {
        ClassMethodSels.push_back((*iter)->getSelector());
        std::string TypeStr;
        Context.getObjCEncodingForMethodDecl((*iter),TypeStr);
        ClassMethodTypes.push_back(MakeConstantString(TypeStr));
    }
    // Collect the names of referenced protocols
    llvm::SmallVector<std::string, 16> Protocols;
    const ObjCList<ObjCProtocolDecl> &Protos =ClassDecl->getReferencedProtocols();
    for (ObjCList<ObjCProtocolDecl>::iterator I = Protos.begin(),
         E = Protos.end(); I != E; ++I)
        Protocols.push_back((*I)->getNameAsString());
    
    
    
    // Get the superclass pointer.
    llvm::Constant *SuperClass;
    if (!SuperClassName.empty()) {
        SuperClass = MakeConstantString(SuperClassName, ".super_class_name");
    } else {
        SuperClass = llvm::ConstantPointerNull::get(PtrToInt8Ty);
    }
    // Empty vector used to construct empty method lists
    llvm::SmallVector<llvm::Constant*, 1>  empty;
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
    const llvm::Type *IndexTy = llvm::Type::getInt32Ty(VMContext);
    llvm::Constant *offsetPointerIndexes[] = {Zeros[0],
        llvm::ConstantInt::get(IndexTy, 1), 0,
        llvm::ConstantInt::get(IndexTy, 2) };
    
    
    for (unsigned i = 0, e = OIvars.size(); i != e; ++i) {
        ObjCIvarDecl *IVD = OIvars[i];
        const std::string Name = "__objc_ivar_offset_" + ClassName + '.'
        + IVD->getNameAsString();
        offsetPointerIndexes[2] = llvm::ConstantInt::get(IndexTy, i);
        // Get the correct ivar field
        llvm::Constant *offsetValue = llvm::ConstantExpr::getGetElementPtr(
                                                                           IvarList, offsetPointerIndexes, 4);
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
        }
    }
    //Generate metaclass for class methods
    llvm::Constant *MetaClassStruct = GenerateClassStructure(NULLPtr,
                                                             NULLPtr, 0x12L, ClassName.c_str(), 0, Zeros[0], GenerateIvarList(
                                                                                                                              empty, empty, empty), ClassMethodList, NULLPtr, NULLPtr, NULLPtr, true);
    
    // Generate the class structure
    llvm::Constant *ClassStruct =
    GenerateClassStructure(MetaClassStruct, SuperClass, 0x11L,
                           ClassName.c_str(), 0,
                           llvm::ConstantInt::get(LongTy, instanceSize), IvarList,
                           MethodList, GenerateProtocolList(Protocols), IvarOffsetArray,
                           Properties);
    
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
    
    std::vector<llvm::Constant*> Elements;
    llvm::Constant *Statics = NULLPtr;
    // Generate statics list:
    if (ConstantStrings.size()) {
        llvm::ArrayType *StaticsArrayTy = llvm::ArrayType::get(PtrToInt8Ty,
                                                               ConstantStrings.size() + 1);
        ConstantStrings.push_back(NULLPtr);
        
        llvm::StringRef StringClass = CGM.getLangOptions().ObjCConstantStringClass;
        
        StringClass = "NSConstantString";
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
    llvm::StructType *SymTabTy = llvm::StructType::get(LongTy, PtrToInt8Ty,
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
        Selectors.push_back(SelName);
        //SelectorAliases.push_back(iter->second);
    }
    
    unsigned SelectorCount = Selectors.size();
    // NULL-terminate the selector list. Cocotron needs this in the current implementation    
    Selectors.push_back(NULLPtr);
    Elements.clear();
        
    
    // Number of static selectors
    Elements.push_back(llvm::ConstantInt::get(LongTy, SelectorCount));
    
    
    llvm::Constant *SelectorList = MakeGlobalArray(PtrToInt8Ty, Selectors,
                                                   ".objc_selector_list");
    
    Elements.push_back(llvm::ConstantExpr::getBitCast(SelectorList,
                                                      PtrToInt8Ty));
    
    // Now that all of the static selectors exist, create pointers to them.
    for (unsigned int i=0 ; i<SelectorCount ; i++) {
        llvm::Constant *Idxs[] = {Zeros[0],
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(VMContext), i), Zeros[0]};
        
        // FIXME: We're generating redundant loads and stores here!
        llvm::Constant *SelPtr = llvm::ConstantExpr::getGetElementPtr(SelectorList,
                                                                      Idxs, 2);
        
        // If selectors are defined as an opaque type, cast the pointer to this
        // type.
        SelPtr = llvm::ConstantExpr::getBitCast(SelPtr, PtrToInt8Ty);
        
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
														(CGM.getLangOptions().getGCMode() == LangOptions::NonGC) ? NULL : IntTy,
														NULL);    
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
	
	switch (CGM.getLangOptions().getGCMode()) {
		case LangOptions::GCOnly:
			Elements.push_back(llvm::ConstantInt::get(IntTy, 2));
		case LangOptions::NonGC:
			break;
		case LangOptions::HybridGC:
			Elements.push_back(llvm::ConstantInt::get(IntTy, 1));				
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

























llvm::Value *CGObjCCocotron::EmitIvarOffset(CodeGenFunction &CGF,
                                       const ObjCInterfaceDecl *Interface,
                                       const ObjCIvarDecl *Ivar) {
    if (CGM.getLangOptions().ObjCNonFragileABI && !CGM.getLangOptions().ObjCNonFragileABI2) {
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