#define CubeEnvPrefilterRS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
    "DescriptorTable(CBV(b0), visibility = SHADER_VISIBILITY_VERTEX)," \
    "DescriptorTable(CBV(b0), CBV(b1), SRV(t0), visibility = SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s0, visibility = SHADER_VISIBILITY_PIXEL," \
        "addressU = TEXTURE_ADDRESS_REPEAT," \
        "addressV = TEXTURE_ADDRESS_REPEAT," \
        "addressW = TEXTURE_ADDRESS_REPEAT," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"
