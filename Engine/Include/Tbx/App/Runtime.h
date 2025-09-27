#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
	/// <summary>
	/// This is a layer added at runtime via the plugin system via a runtime loader.
	/// </summary>
	class TBX_EXPORT Runtime : public Tbx::Layer
	{
	public:
		Runtime(const std::string& name);

		/// <summary>
		/// Starts a runtime.
		/// Should be called after important systems are initialized so the runtime can utilize them.
		/// </summary>
		void Initialize();

		/// <summary>
		/// Shuts down a runtime.
		/// </summary>
		void Shutdown();

		// This occurs BEFORE our app has been fully initialized. So we don't want to do anything here... 
		// overriding to hide it from inheritors
		void OnAttach() final;

		// Shutdown makes more sense so we will override this to hide from inheritors, it will just call on shutdown.
		void OnDetach() final;

	protected:
		virtual void OnStart() {}
		virtual void OnShutdown() {}
	};

	/// <summary>
	/// Register a runtime
	/// </summary>
	#define TBX_REGISTER_RUNTIME(runtimeType) \
		class runtimeType##RuntimeLoader : public Tbx::Plugin\
		{\
		public:\
			runtimeType##RuntimeLoader(Tbx::WeakRef<Tbx::App> app)\
			{\
				app.lock()->AddLayer<runtimeType>(app);\
			}\
		};\
		TBX_REGISTER_PLUGIN(runtimeType##RuntimeLoader)
}