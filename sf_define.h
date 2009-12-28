
#define GSH_SF_ZERO(base, spec)  _GSH_SF_U(base ## _zero_ ## spec, base ## spec ## zero)
#define GSH_SF_ZERO_D(base, spec)  _GSH_SF_DU(base ## _zero_ ## spec, base ## spec ## zero)

#define GSH_SF_D(base, spec)  _GSH_SF_D(base ## _ ## spec, base ## spec)
#define GSH_SF_D_SIMPLE(base) _GSH_SF_D(base, base)
#define GSH_SF_U_SIMPLE(base) _GSH_SF_U(base, base)
#define GSH_SF_UU_SIMPLE(base) _GSH_SF_UU(base, base)
#define GSH_SF_D_SUFFIX(base, spec, sfx) _GSH_SF_D(base ## _ ## spec ## _ ## sfx
#define GSH_SF_D_MODE(base, spec)  _GSH_SF_D_MODE(base ## _ ## spec, base ## spec)

#define GSH_SF_DD(base, spec)  _GSH_SF_DD(base ## _ ## spec, base ## spec)
#define GSH_SF_DD_SIMPLE(base) _GSH_SF_DD(base, base)
#define GSH_SF_DD_SUFFIX(base, spec, sfx) _GSH_SF_DD(base ## _ ## spec ## _ ## sfx, base)

#define GSH_SF_DDD(base, spec)  _GSH_SF_DDD(base ## _ ## spec, base ## spec)
#define GSH_SF_DDD_SIMPLE(base) _GSH_SF_DDD(base, base)
#define GSH_SF_DDD_SUFFIX(base, spec, sfx) _GSH_SF_DDD(base ## _ ## spec ## _ ## sfx, base)

#define GSH_SF_ID(base, spec)  _GSH_SF_ID(base ## _ ## spec, base ## spec)
#define GSH_SF_ID_SIMPLE(base) _GSH_SF_ID(base, base)
#define GSH_SF_ID_SUFFIX(base, spec, sfx) _GSH_SF_ID(base ## _ ## spec ## _ ## sfx, base)

#define GSH_SF_IID(base, spec)  _GSH_SF_IID(base ## _ ## spec, base ## spec)
#define GSH_SF_IID_SIMPLE(base) _GSH_SF_IID(base, base)
#define GSH_SF_IID_SUFFIX(base, spec, sfx) _GSH_SF_IID(base ## _ ## spec ## _ ## sfx, base)


#define GGSL_SF_NAME(nm) gsl_sf_ ## nm ## _e
