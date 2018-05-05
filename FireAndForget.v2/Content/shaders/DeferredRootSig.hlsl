#define DeferredRootSig "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
			"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
		"DescriptorTable(CBV(b0, flags = DATA_STATIC)," \
						"SRV(t0, numDescriptors = 5," \
							"flags = DESCRIPTORS_VOLATILE))," \
		"StaticSampler(s0," \
			"addressU = TEXTURE_ADDRESS_CLAMP," \
			"addressV = TEXTURE_ADDRESS_CLAMP," \
			"filter = FILTER_MIN_MAG_MIP_POINT," \
			"visibility = SHADER_VISIBILITY_PIXEL)"
