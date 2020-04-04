#include <angelscript-llvm/detail/modulebuilder.hpp>

#include <angelscript-llvm/detail/functionbuilder.hpp>
#include <angelscript-llvm/detail/jitcompiler.hpp>
#include <angelscript-llvm/detail/llvmglobals.hpp>
#include <angelscript-llvm/detail/modulecommon.hpp>
#include <array>
#include <fmt/core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>

namespace asllvm::detail
{
ModuleBuilder::ModuleBuilder(JitCompiler& compiler, std::string_view angelscript_module_name) :
	m_compiler{compiler},
	m_module{std::make_unique<llvm::Module>(make_module_name(angelscript_module_name), compiler.builder().context())}
{}

void ModuleBuilder::add_jit_function(std::string name, asJITFunction* function)
{
	m_jit_functions.emplace_back(std::move(name), function);
}

FunctionBuilder ModuleBuilder::create_function_builder(asCScriptFunction& function)
{
	return {m_compiler, *this, function, create_function(function)};
}

llvm::Function* ModuleBuilder::create_function(asCScriptFunction& function)
{
	if (auto it = m_script_functions.find(function.GetId()); it != m_script_functions.end())
	{
		return it->second;
	}

	std::array<llvm::Type*, 1> types{llvm::PointerType::getInt32PtrTy(m_compiler.builder().context())};
	llvm::Type*                return_type = m_compiler.builder().to_llvm_type(function.returnType);

	llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, types, false);

	const std::string name = make_function_name(function.GetName(), function.GetNamespace());

	llvm::Function* llvm_function
		= llvm::Function::Create(function_type, llvm::Function::InternalLinkage, name, *m_module.get());

	(llvm_function->arg_begin() + 0)->setName("params");

	// TODO: fix this, but how to CreateCall with this convention?! in functionbuilder.cpp
	// llvm_function->setCallingConv(llvm::CallingConv::Fast);

	m_script_functions.emplace(function.GetId(), llvm_function);

	return llvm_function;
}

llvm::Function* ModuleBuilder::get_system_function(asCScriptFunction& system_function)
{
	const int id = system_function.GetId();

	if (auto it = m_system_functions.find(id); it != m_system_functions.end())
	{
		return it->second;
	}

	llvm::Type* return_type = m_compiler.builder().to_llvm_type(system_function.returnType);

	const std::size_t        param_count = system_function.GetParamCount();
	std::vector<llvm::Type*> types(param_count);

	for (std::size_t i = 0; i < param_count; ++i)
	{
		types[i] = m_compiler.builder().to_llvm_type(system_function.parameterTypes[i]);
	}

	llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, types, false);

	llvm::Function* function = llvm::Function::Create(
		function_type, llvm::Function::ExternalLinkage, 0, make_system_function_name(system_function), m_module.get());

	m_system_functions.emplace(id, function);

	return function;
}

void ModuleBuilder::build()
{
	if (m_compiler.config().verbose)
	{
		dump_state();
	}

	m_compiler.builder().optimizer().run(*m_module);

	for (const auto& it : m_system_functions)
	{
		auto& script_func = static_cast<asCScriptFunction&>(*m_compiler.engine().GetFunctionById(it.first));

		llvm::JITTargetAddress address = {};
		std::memcpy(&address, &script_func.sysFuncIntf->func, sizeof(asFUNCTION_t));
		llvm::JITEvaluatedSymbol symbol(address, llvm::JITSymbolFlags::Callable);

		ExitOnError(m_compiler.jit().defineAbsolute(it.second->getName(), symbol));
	}

	ExitOnError(m_compiler.jit().addIRModule(
		llvm::orc::ThreadSafeModule(std::move(m_module), m_compiler.builder().extract_old_context())));

	for (auto& pair : m_jit_functions)
	{
		auto entry   = ExitOnError(m_compiler.jit().lookup(pair.first + ".jitentry"));
		*pair.second = reinterpret_cast<asJITFunction>(entry.getAddress());
	}
}

void ModuleBuilder::dump_state() const
{
	for (const auto& function : m_module->functions())
	{
		fmt::print(stderr, "Function '{}'\n", function.getName().str());
	}

	m_module->print(llvm::errs(), nullptr);
}
} // namespace asllvm::detail
