#include <angelscript-llvm/detail/builder.hpp>

#include <angelscript-llvm/detail/llvmglobals.hpp>
#include <angelscript-llvm/detail/modulecommon.hpp>
#include <angelscript.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

namespace asllvm::detail
{
Builder::Builder(JitCompiler& compiler) :
	m_context{setup_context()},
	m_compiler{compiler},
	m_pass_manager{setup_pass_manager()},
	m_ir_builder{*m_context},
	m_common_definitions{setup_common_definitions()}
{}

llvm::Type* Builder::script_type_to_llvm_type(int type_id) const
{
	switch (type_id)
	{
	case asTYPEID_VOID: return llvm::Type::getVoidTy(*m_context);
	case asTYPEID_BOOL: return llvm::Type::getInt1Ty(*m_context);
	case asTYPEID_INT8: return llvm::Type::getInt8Ty(*m_context);
	case asTYPEID_INT16: return llvm::Type::getInt16Ty(*m_context);
	case asTYPEID_INT32: return llvm::Type::getInt32Ty(*m_context);
	case asTYPEID_INT64: return llvm::Type::getInt64Ty(*m_context);
	default:
	{
		throw std::runtime_error{"type not implemented"};
	}
	}
}

bool Builder::is_script_type_64(int type_id) const
{
	switch (type_id)
	{
	case asTYPEID_INT64:
	case asTYPEID_DOUBLE:
	{
		return true;
	}

	case asTYPEID_VOID:
	case asTYPEID_BOOL:
	case asTYPEID_INT8:
	case asTYPEID_INT16:
	case asTYPEID_INT32:
	{
		return false;
	}

	default: throw std::runtime_error{"type not implemented"};
	}
}

std::size_t Builder::get_script_type_dword_size(int type_id) const { return is_script_type_64(type_id) ? 2 : 1; }

llvm::legacy::PassManager& Builder::optimizer() { return m_pass_manager; }

llvm::LLVMContext& Builder::context() { return *m_context; }

std::unique_ptr<llvm::LLVMContext> Builder::extract_old_context()
{
	auto old_context = std::move(m_context);

	m_context            = setup_context();
	m_common_definitions = setup_common_definitions();

	return old_context;
}

CommonDefinitions Builder::setup_common_definitions()
{
	CommonDefinitions defs;

	defs.i1  = llvm::Type::getInt1Ty(*m_context);
	defs.i8  = llvm::Type::getInt8Ty(*m_context);
	defs.i16 = llvm::Type::getInt16Ty(*m_context);
	defs.i32 = llvm::Type::getInt32Ty(*m_context);
	defs.i64 = llvm::Type::getInt64Ty(*m_context);

	defs.pvoid = llvm::Type::getInt8PtrTy(*m_context);
	defs.pi8   = llvm::Type::getInt8PtrTy(*m_context);
	defs.pi16  = llvm::Type::getInt16PtrTy(*m_context);
	defs.pi32  = llvm::Type::getInt32PtrTy(*m_context);
	defs.pi64  = llvm::Type::getInt64PtrTy(*m_context);

	{
		std::array<llvm::Type*, 8> types{{
			defs.pi32,  // programPointer
			defs.pi32,  // stackFramePointer
			defs.pi32,  // stackPointer
			defs.i64,   // valueRegister
			defs.pvoid, // objectRegister
			defs.pvoid, // objectType - todo asITypeInfo
			defs.i1,    // doProcessSuspend
			defs.pvoid, // ctx - todo asIScriptContext
		}};

		defs.vm_registers = llvm::StructType::create(types, "asSVMRegisters");
	}

	return defs;
}

std::unique_ptr<llvm::LLVMContext> Builder::setup_context() { return std::make_unique<llvm::LLVMContext>(); }

llvm::legacy::PassManager Builder::setup_pass_manager()
{
	llvm::PassManagerBuilder pmb;
	pmb.OptLevel           = 3;
	pmb.Inliner            = llvm::createFunctionInliningPass(275);
	pmb.DisableUnrollLoops = false;
	pmb.LoopVectorize      = true;
	pmb.SLPVectorize       = true;

	llvm::legacy::PassManager pm;
	pm.add(llvm::createVerifierPass());
	pmb.populateModulePassManager(pm);
	pm.add(llvm::createVerifierPass());

	return pm;
}
} // namespace asllvm::detail
