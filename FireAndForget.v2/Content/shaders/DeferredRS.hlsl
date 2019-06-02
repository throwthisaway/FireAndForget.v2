#define DeferredRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
			"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
		"DescriptorTable(CBV(b0)," \
						"CBV(b1)," \
						"SRV(t0, numDescriptors = 10), visibility = SHADER_VISIBILITY_PIXEL)" \
		"StaticSampler(s0," \
			"addressU = TEXTURE_ADDRESS_CLAMP," \
			"addressV = TEXTURE_ADDRESS_CLAMP," \
			"addressW = TEXTURE_ADDRESS_CLAMP," \
			"filter = FILTER_MIN_MAG_MIP_POINT," \
			"visibility = SHADER_VISIBILITY_PIXEL)"
		"StaticSampler(s1," \
			"addressU = TEXTURE_ADDRESS_REPEAT," \
			"addressV = TEXTURE_ADDRESS_REPEAT," \
			"addressW = TEXTURE_ADDRESS_REPEAT," \
			"filter = FILTER_MIN_MAG_MIP_LINEAR," \
			"visibility = SHADER_VISIBILITY_PIXEL)"
		"StaticSampler(s2," \
			"addressU = TEXTURE_ADDRESS_CLAMP," \
			"addressV = TEXTURE_ADDRESS_CLAMP," \
			"addressW = TEXTURE_ADDRESS_CLAMP," \
			"filter = FILTER_MIN_MAG_MIP_LINEAR," \
			"visibility = SHADER_VISIBILITY_PIXEL)"
