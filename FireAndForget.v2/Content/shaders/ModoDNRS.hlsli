#define ModoDNRS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
    "DescriptorTable(CBV(b0), visibility = SHADER_VISIBILITY_VERTEX)," \
    "DescriptorTable(SRV(t0, numDescriptors = 2), CBV(b0),visibility = SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s0, visibility = SHADER_VISIBILITY_PIXEL," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"