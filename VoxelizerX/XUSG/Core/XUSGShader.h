//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGType.h"

namespace XUSG
{
	namespace Shader
	{
		namespace Stage
		{
			enum Type : uint8_t
			{
				VS,
				PS,
				DS,
				HS,
				GS,
				CS,
				ALL = CS,

				NUM_GRAPHICS = ALL,
				NUM,
			};
		}

		class Pool
		{
		public:
			Pool();
			virtual ~Pool();

			void SetShader(Stage::Type stage, uint32_t index, const Blob &shader);
			void SetShader(Stage::Type stage, uint32_t index, const Blob &shader, const Reflector &reflector);
			void SetReflector(Stage::Type stage, uint32_t index, const Reflector &reflector);

			Blob		CreateShader(Stage::Type stage, uint32_t index, const std::wstring &fileName);
			Blob		GetShader(Stage::Type stage, uint32_t index) const;
			Reflector	GetReflector(Stage::Type stage, uint32_t index) const;

		protected:
			Blob		&checkShaderStorage(Stage::Type stage, uint32_t index);
			Reflector	&checkReflectorStorage(Stage::Type stage, uint32_t index);

			std::vector<Blob>		m_shaders[Stage::NUM];
			std::vector<Reflector>	m_reflectors[Stage::NUM];
		};
	}
}
