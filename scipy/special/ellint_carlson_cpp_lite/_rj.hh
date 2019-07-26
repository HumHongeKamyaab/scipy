#ifndef ELLINT_RJ_GENERIC_GUARD
#define ELLINT_RJ_GENERIC_GUARD


#include <algorithm>
#include <iterator>
#include <complex>
#include <cmath>
#include <cstring>
#include "ellint_typing.hh"
#include "ellint_argcheck.hh"
#include "ellint_common.hh"
#include "ellint_carlson.hh"


/* Forward declaration */
namespace ellint_carlson
{
template<typename T>
ExitStatus
rj(const T& x, const T& y, const T& z, const T& p, const double& rerr, T& res);


template<typename T>
ExitStatus
rc(const T& x, const T& y, const double& rerr, T& res);


template<typename T>
ExitStatus
rg(const T& x, const T& y, const T& z, const double& rerr, T& res);
}


#define ELLINT_RJ_CALC()	\
do { 	\
    prm = std::sqrt(pm);	\
    cct1[0] = cct2[1] = std::sqrt(xm);	\
    cct1[1] = cct2[2] = std::sqrt(ym);	\
    cct1[2] = cct2[0] = std::sqrt(zm);	\
    lam = arithmetic::ndot2(cct1, cct2, 3);	\
    dm = (prm + cct1[0]) * (prm + cct1[1]) * (prm + cct1[2]);\
} while ( 0 )

#define ELLINT_RJ_UPDT()	\
do {	\
    Am = (Am + lam) * (RT)0.25;	\
    xm = (xm + lam) * (RT)0.25;	\
    ym = (ym + lam) * (RT)0.25;	\
    zm = (zm + lam) * (RT)0.25;	\
    pm = (pm + lam) * (RT)0.25;	\
    xxm = xxm * (RT)0.25;	\
    yym = yym * (RT)0.25;	\
    zzm = zzm * (RT)0.25;	\
    d4m *= (RT)0.25;	\
    fterm *= (RT)0.25;	\
} while ( 0 )



namespace ellint_carlson { namespace rjimpl
{

template<typename T>
static inline constexpr typing::real_only<T, bool>
asymp_zero(const T& r)
{
    return ( (0.0 < r) && (r <= config::asym_zero_ul) );
}

template<typename T>
static inline constexpr typing::real_only<T, bool>
abs_close_zero(const T& r)
{
    return ( (0.0 < r) && (r <= config::asym_close_ul) );
}


enum class AsymFlag
{
    nothing,
    hugep,	/* x, y, z << p */
    tinyp,	/* p << geom. mean of x, y, z */
    hugey,	/* max(x, p) << min(y, z) == y */
    tinyx,	/* x << min(y, z, p) == min(y, p) */
    tinyy,	/* max(x, y) == y << min(z, p) */
    hugez	/* max(x, y, p) == max(y, p) << z */
};


struct ArgCases
{
    bool maybe_asymp;	/* might be good for an asympt. case */
    bool retry_caupv;	/* should use Cauchy principal value */
    bool good_infinity;	/* the "good" kind of directed infinity */
    bool hit_pole;	/* singular */
};


/* Comparison function by the real part. */
template<typename T>
static inline bool rcmp(const T& a, const T& b)
{
    return ( std::real(a) < std::real(b) );
}


/* Check whether the input arguments for RJ are out of domain, while set
 * corresponding flags in the class(ification) variable.
 *
 * NOTE: x, y, z must be in-order by real parts.
 */
template<typename T>
static bool
good_args(const T& x, const T& y, const T& z, const T& p,
          ArgCases& classify)
{
    typedef typing::decplx_t<T> RT;

    if ( (classify.hit_pole = ( argcheck::too_small(x) &&
                                argcheck::too_small(y) &&
			        argcheck::ph_good(z) &&
			        !argcheck::too_small(p) )) )
    {
	return false;
    }
    if ( (classify.good_infinity = ( (argcheck::isinf(x) ||
				      argcheck::isinf(y) ||
				      argcheck::isinf(z) ||
				      argcheck::isinf(p)) &&
                                     (argcheck::ph_good(x) &&
				      argcheck::ph_good(y) &&
				      argcheck::ph_good(z)) )) )
    {
	return false;
    }
    RT xr = std::real(x);
    RT xi = std::imag(x);

    RT yr = std::real(y);
    RT yi = std::imag(y);

    RT zi = std::imag(z);

    RT pr = std::real(p);
    RT pi = std::imag(p);

    /* "If x, y, z are real and nonnegative, at most one of them is 0, and the
     * fourth variable of RJ is negative, the Cauchy principal value ..." */
    bool xyzreal_nonneg_atmost1z = ( argcheck::too_small(xi) &&
                                     argcheck::too_small(yi) &&
                                     argcheck::too_small(zi) &&
				     (xr >= 0.0) && (yr > 0.0) );
    if ( argcheck::too_small(pi) && xyzreal_nonneg_atmost1z )
    {
	if ( (classify.retry_caupv = ( pr < 0.0 )) )
	{
	    return false;
	}
	/* "Assume x, y, and z are real and nonnegative, at most one of them is
	 * 0, and p > 0" */
	if ( (classify.maybe_asymp = ( pr > 0.0 )) )
	{
	    return true;
	}
    }
    /* "Let x, y, z have nonnegative real part and at most one of them [1] be
     * 0, while Re p > 0."
     *     [1] By "them", Carlson seems to have meant the numbers x, y, z
     *         themselves, rather than their "real parts". */
    bool x0 = argcheck::too_small(x);
    bool y0 = argcheck::too_small(y);
    bool z0 = argcheck::too_small(z);
    if ( ( pr > 0.0 ) && ( xr >= 0.0 ) && !((y0 && (x0 || z0)) || (x0 && z0)) )
    {
	return true;
    }
    /* "Alternatively, if p != 0 and |ph p| < pi ..." */
    if ( (!argcheck::too_small(p)) && argcheck::ph_good(p) )
    {
	/* "... either let x, y, z be real and nonnegative and at most one of
	 * them be 0, ..." */
	unsigned char flag = 0u;
	if (  xyzreal_nonneg_atmost1z )
	{
	    flag ^= 1u;
	}
	/* "... or else let two of the variables x, y, z be nonzero and
	 * conjugate complex with phase less in magnitude than pi and the
	 * third variable be real and nonnegative." */
	if ( argcheck::r1conj2(x, y, z) ||
	     argcheck::r1conj2(y, z, x) ||
	     argcheck::r1conj2(z, x, y) )
	{
	    flag ^= 1u;
	}
	return (bool)flag;
    }
    return false;
}


/* Cauchy principal value dispatcher */
template<typename T, typename Tres>
static ExitStatus
rj_cpv_dispatch(const T& x, const T& y, const T& z, const T& p,
		const double& rerr, Tres& res)
{
    /* Retry with principal value evaluation, valid for reals. */
    ExitStatus status, status_tmp;
    T xct1[4] = {x, y, -p, z};
    T xct2[4];

    double r = rerr / 3.0;
    T xy = xct1[0] * xct1[1];
    xct2[3] = xct1[2] / xct1[3] + (T)1.0;
    T pn = (arithmetic::nsum2(xct1, 3) - xy / xct1[3]) / xct2[3];

    status = ExitStatus::success;
    status_tmp = rj(xct1[0], xct1[1], xct1[3], pn, r, xct2[0]);
    if ( is_horrible(status_tmp) )
    {
	return status_tmp;
    } else if ( is_troublesome(status_tmp) ) {
	status = status_tmp;
    }

    status_tmp = rf(xct1[0], xct1[1], xct1[3], r, xct2[1]);
    if ( is_horrible(status_tmp) )
    {
	return status_tmp;
    } else if ( is_troublesome(status_tmp) ) {
	status = status_tmp;
    }

    T pq = pn * xct1[2];
    T xypq = xy + pq;
    status_tmp = rc(xypq, pq, r, xct2[2]);
    if ( is_horrible(status_tmp) )
    {
	return status_tmp;
    } else if ( is_troublesome(status_tmp) ) {
	status = status_tmp;
    }
    xct1[0] = pn / xct1[3] - (T)1.0;
    xct1[1] = -(T)3.0 / xct1[3];
    xct1[2] = (T)3.0 * std::sqrt(xy / (xypq * xct1[3]));

    /* tmpres = (pn - zz) * rjv - 3.0 * (rfv - sqrt(xy * zz / xypq) * rcv); */
    T tmpres = arithmetic::ndot2(xct1, xct2, 3);
    /* tmpres /= q + zz */
    tmpres /= xct2[3];
    res = (Tres)tmpres;

    return status;
}


template<typename T>
struct AsymConfig
{
    T a;
    T b;
    T c;
    T f;
    T g;
    T h;
};

template<typename T>
static typing::real_only<T, AsymFlag>
rj_asym_conf(const T& x, const T& y, const T& z, const T& p,
             AsymConfig<T>& conf)
{
    T t;

    /*
    t = (*z) / (*p);
    */
    /* this bound is neither sharp enough nor useful */
    /*
    if ( ASYMP_ZERO(t) )
    {
	*c = ((*x) + (*y) + (*z)) / 3.0;
	return asymp_hugep;
    }
    */

    /* this bound is sharp. RJ in this case behaves with logarithmic
     * singularity as p -> +0 */
    if ( abs_close_zero(p) || ( (x != 0.0) && asymp_zero(p / x) ) )
    {
	conf.f = std::sqrt(x * y * z);
	return AsymFlag::tinyp;
    }

    /* XXX until RJ(0, y, z, p) is implemented, this is useless */
    /*
    t = (*x) / fmin((*y), (*p));
    if ( ( 0.0 < (*x) && (*x) <= 1e-26 ) || ASYMP_ZERO(t) )
    {
	*h = sqrt((*y) * (*z));
	if ( (*h) / (*p) + 0.5 * ((*y) + (*z)) / (*h) <= sqrt((*h) / (*x)) )
	{
	    return asymp_tinyx;
	}
    }
    */

    t = y / std::fmin(z, p);
    if ( ( (0.0 < y) && (y <= 1e-26) ) || asymp_zero(t) )
    {
	/* bound fairly sharp even if p is large */
	conf.a = (T)0.5 * (x + y);
	conf.g = std::sqrt(x * y);
	if ( (conf.a / z + conf.a / p) * std::abs(std::log(p / conf.a)) <=
	     1.0 )
	{
	    return AsymFlag::tinyy;
	}
    }

    if ( (x != 0.0) && asymp_zero(std::fmax(z, p) / x) )
    {
	/* bound might not be sharp if x + 2p much larger than (yz) ** 2,
	 * but this is unlikely to be true anyway. */
	return AsymFlag::hugey;
    }

    if ( (z != 0.0) && asymp_zero(std::fmax(y, p) / z) )
    {
	conf.b = 0.5 * (x + y);
	conf.h = std::sqrt(x * y);
	/* when bounds are sharp */
	if ( std::abs(std::log(z / (conf.b + conf.h))) <= std::sqrt(z) )
	{
	    return AsymFlag::hugez;
	}
    }

    return AsymFlag::nothing;
}


/* Prevent division by zero due to underflow in atan(sqrt(z)) / sqrt(z) and
 * square root of negative number in the real context. */
template<typename T>
static inline typing::cplx_only<T, T>
safe_atan_sqrt_div(T z)
{
    if ( argcheck::too_small(z) )
    {
	return T(1.0);
    }
    T s = std::sqrt(z);
    return std::atan(s) / s;
}

template<typename T>
static inline typing::real_only<T, T>
safe_atan_sqrt_div(T x)
{
    if ( argcheck::too_small(x) )
    {
	return T(1.0);
    }
    T s;
    if ( x < 0.0 )
    {
	s = std::sqrt(-x);
	return std::atanh(s) / s;
    }
    s = std::sqrt(x);
    return std::atan(s) / s;
}


}  /* namespace ellint_carlson::rjimpl */

template<typename T>
ExitStatus
rj(const T& x, const T& y, const T& z, const T& p, const double& rerr, T& res)
{
    typedef typing::decplx_t<T> RT;

    rjimpl::ArgCases classify;
    T cct1[6];
    T cct2[6];

    ExitStatus status = ExitStatus::success;
#ifndef ELLINT_NO_VALIDATE_RELATIVE_ERROR_BOUND
    if ( argcheck::invalid_rerr(rerr, 1.0e-4) )
    {
	res = typing::nan<T>();
	return ExitStatus::bad_rerr;
    }
#endif

    /* Put the symmetric arguments in the order of real parts. */
    cct1[0] = x;
    cct1[1] = y;
    cct1[2] = z;
    auto b = std::begin(cct1);
    std::sort(b, b + 3, rjimpl::rcmp<T>);
    T xm = cct1[0];
    T ym = cct1[1];
    T zm = cct1[2];
    std::memset(&classify, 0, sizeof classify);
    if ( !rjimpl::good_args(xm, ym, zm, p, classify) )
    {
	if ( classify.good_infinity )
	{
	    res = (T)0.0;
	    return ExitStatus::success;
	}

	if ( classify.retry_caupv )
	{
	    /* Retry with principal value evaluation, valid for reals. */
	    RT tmpres;
	    RT xr(std::real(xm));
	    RT yr(std::real(ym));
	    RT zr(std::real(zm));
	    RT pr(std::real(p));
	    status = rjimpl::rj_cpv_dispatch(xr, yr, zr, pr, rerr, tmpres);
	    if ( is_horrible(status) )
	    {
		res = typing::nan<T>();
	    } else {
		res = T(tmpres);
	    }
	    return status;
	} else if ( classify.hit_pole ) {
	    res = typing::huge<T>();
	    return ExitStatus::singular;
	} else {
	    res = typing::nan<T>();
	    return ExitStatus::bad_args;
	}
    }

    if ( classify.maybe_asymp )
    {
	/* might be dealt with by asymptotic expansion of real-arg RJ */
	RT tmpres;
	rjimpl::AsymConfig<RT> config;
	RT xr(std::real(xm));
	RT yr(std::real(ym));
	RT zr(std::real(zm));
	RT pr(std::real(p));
	rjimpl::AsymFlag cres = rjimpl::rj_asym_conf(xr, yr, zr, pr, config);
	switch ( cres )
	{
	    case rjimpl::AsymFlag::nothing : break;
	    case rjimpl::AsymFlag::hugep :
	    {
		status = rf(xr, yr, zr, rerr, tmpres);
		tmpres = 3.0 * (tmpres -
			        0.5 * (RT)(constants::pi) / std::sqrt(pr)) / pr;
		break;
	    }
	    case rjimpl::AsymFlag::tinyp :
	    {
		ExitStatus status_tmp;
		RT xct1[3];
		RT xct2[3];
		RT xct3[3];
		double r = rerr * 0.5;
		xct1[1] = xct2[0] = std::sqrt(xr);
		xct1[2] = xct2[1] = std::sqrt(yr);
		xct1[0] = xct2[2] = std::sqrt(zr);
		RT lamt = arithmetic::dot2(xct1, xct2);
		xct3[0] = xct3[1] = xct3[2] = pr;
		RT alpha = arithmetic::dot2(xct1, xct3) + config.f;
		alpha = alpha * alpha;
		RT beta = pr + lamt;
		beta = beta * beta * pr;
		status_tmp = rc(alpha, beta, r, xct2[0]);
		status = rj(xr + lamt, yr + lamt, zr + lamt, pr + lamt, r,
		            xct2[1]);
		if ( status_tmp != ExitStatus::success )
		{
		    status = status_tmp;
		}
		xct1[0] = (RT)3.0;
		xct1[1] = (RT)2.0;
		tmpres = arithmetic::ndot2(xct1, xct2, 2);
		break;
	    }
	    case rjimpl::AsymFlag::hugey :
	    {
		ExitStatus status_tmp;
		RT t1, t2;
		double r = rerr / 3.0;
		tmpres = (RT)1.0 / std::sqrt(yr * zr);
		status_tmp = rc(xr, pr, r, t1);
		status = rg((RT)0.0, yr, zr, r, t2);
		if ( status_tmp != ExitStatus::success )
		{
		    status = status_tmp;
		}
		tmpres *= ((RT)3.0 * t1 - (RT)2.0 * t2 * tmpres);
		break;
	    }
	    case rjimpl::AsymFlag::tinyx :
	    {
		status = rj((RT)0.0, yr, zr, pr, rerr, tmpres);
		tmpres -= (RT)3.0 * std::sqrt(xr) / (config.h * pr);
		break;
	    }
	    case rjimpl::AsymFlag::tinyy :
	    {
		RT tx;
		status = rc((RT)1.0, pr / zr, rerr, tx);
		tmpres = std::log((RT)8.0 * zr / (config.a + config.g)) -
		         (RT)2.0 * tx;
		tx = std::log((RT)2.0 * pr / (config.a + config.g)) /
		     (tmpres * pr);
		RT r_est_l = tx * config.g / ((RT)1.0 - config.g / pr);
		RT r_est_h = tx * config.a *
		             ((RT)1.0 + (RT)0.5 * pr / zr) / ((RT)1.0 -
			                                      config.a / pr);
		/* if asymptotic expansion found to violate error bound
		 * after the fact */
		if ( r_est_h - r_est_l >= 2.0 * rerr )
		{
		    cres = rjimpl::AsymFlag::nothing;
		    status = ExitStatus::success;
		    break;
		} else {
		    tmpres += r_est_l;
		    tmpres *= (RT)1.5 / (std::sqrt(zr) * pr);
		}
		break;
	    }
	    case rjimpl::AsymFlag::hugez :
	    {
		RT tt = config.h + pr;
		tt *= tt;
		status = rc(tt, (RT)2.0 * (config.b + config.h) * pr, rerr,
		            tmpres);
		RT r_est = (RT)0.25 *
			   ((RT)0.5 + std::log1p((RT)2.0 * zr /
			                         std::sqrt(config.h * pr))) /
			   (tmpres * zr);
		/* if asymptotic expansion found to violate error bound
		 * after the fact */
		if ( r_est >= rerr )
		{
		    cres = rjimpl::AsymFlag::nothing;
		    status = ExitStatus::success;
		    break;
		} else {
		    tmpres *= (RT)3.0 / std::sqrt(zr);
		}
		break;
	    }
	}  /* end of switch */
	if ( cres != rjimpl::AsymFlag::nothing )
	{
	    res = tmpres;
	    return status;
	}
    }  /* end of condition on whether one should try asymptotic approx. */

    cct1[3] = p;
    cct1[4] = p;
    T Am = arithmetic::nsum2(cct1, 5) / (RT)5.0;
    T delta = (p - xm) * (p - ym) * (p - zm);
    T xxm = Am - xm;
    T yym = Am - ym;
    T zzm = Am - zm;
    RT fterm = std::max({std::abs(xxm), std::abs(yym), std::abs(zzm),
			 std::abs(Am - p)}) / arithmetic::ocrt(rerr / 5.0);

    /* m = 0; */
    RT d4m = 1.0;
    T pm = p;
    T prm, lam, dm;
    ELLINT_RJ_CALC();
    T sm = dm * (RT)0.5;
    /* next */
    ELLINT_RJ_UPDT();

    RT aAm;
    unsigned int m = 1;
    while ( (aAm = std::abs(Am)) <= fterm ||
	    aAm <= std::max({std::abs(xxm), std::abs(yym), std::abs(zzm),
                             std::abs(Am - pm)}) )  
    {
	if ( m > config::max_iter )
	{
	    status = ExitStatus::n_iter;
	    break;
	}
	T rm = sm * (std::sqrt((delta * d4m) / (sm * sm) + (RT)1.0) +
	                 (RT)1.0);
	ELLINT_RJ_CALC();
	sm = (rm * dm - delta * (d4m * d4m)) * (RT)0.5 / (dm + rm * d4m);

	/* next */
	ELLINT_RJ_UPDT();
	++m;
    }
    /* Burn some extra cycles re-balancing Am as the "true" centroid */
    cct1[0] = xm;
    cct1[1] = ym;
    cct1[2] = zm;
    cct1[3] = pm;
    cct1[4] = pm;
    Am = arithmetic::nsum2(cct1, 5) / (RT)5.0;
    xxm /= Am;
    yym /= Am;
    zzm /= Am;
    /* pp = -0.5 * (xxm + yym + zzm) */
    cct1[0] = cct2[2] = xxm;
    cct1[1] = cct2[0] = yym;
    cct1[2] = cct2[1] = zzm;
    T pp = arithmetic::nsum2(cct1, 3) * (RT)(-0.5);
    cct1[3] = cct2[3] = pp;
    cct1[3] = cct1[3] * (RT)(-3.0);
    T pp2 = pp * pp;
    T xyz = yym * zzm * xxm;
    /* e2 = xxm * yym + zzm * xxm + yym * zzm - pp2 * 3.0 */
    T e2 = arithmetic::ndot2(cct1, cct2, 4);
    /* e3 = xyz + 2.0 * pp * (e2 + 2.0 * pp2) */
    T e3 = xyz + pp * (RT)2.0 * (e2 + pp2 * (RT)2.0);
    /* e4 = (2.0 * xyz + (e2 + 3.0 * pp2) * pp) * pp */
    T e4 = ((RT)2.0 * xyz + (e2 + (RT)3.0 * pp2) * pp) * pp;
    T e5 = xyz * pp2;
    /* tmp = d4m * pow(sqrt(Am), -3) */
    T t = std::sqrt(Am);
    T tmp = d4m / (t * t * t);
    cct1[0] = arithmetic::comp_horner(e2, constants::RDJ_C1);
    cct1[1] = arithmetic::comp_horner(e3, constants::RDJ_C2);
    cct1[2] = arithmetic::comp_horner(e2, constants::RDJ_C3);
    cct1[3] = arithmetic::comp_horner(e2, constants::RDJ_C4);
    cct1[4] = arithmetic::comp_horner(e2, constants::RDJ_C5);
    cct1[5] = e3 * (RT)(constants::RDJ_C5[1]);

    cct2[0] = T(1.0);
    cct2[1] = T(1.0);
    cct2[2] = e3;
    cct2[3] = e4;
    cct2[4] = e5;
    cct2[5] = e4;
    t = arithmetic::dot2(cct1, cct2) / (RT)(constants::RDJ_DENOM) + (RT)1.0;
    tmp *= t;
    t = delta * d4m / (sm * sm);
    tmp += rjimpl::safe_atan_sqrt_div(t) * (RT)3.0 / sm;

    res = tmp;
    return status;
}


}  /* namespace ellint_carlson */


#endif /* ELLINT_RJ_GENERIC_GUARD */
