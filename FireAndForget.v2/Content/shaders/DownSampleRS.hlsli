#define DownsampleRS \
    "RootFlags(DENY_VERTEX_SHADER_ROOT_ACCESS |" \
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
    "DescriptorTable(CBV(b0)," \
					"SRV(t0)," \
					"visibility = SHADER_VISIBILITY_PIXEL)"
