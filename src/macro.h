#ifdef NDEBUG
    // Release version
#else
    // Debug version
    #define RARELY_USED
    #ifdef COMPILE_ONLY
        #define SAFE_CASTING
    #endif
#endif

// Replace NULL with nullptr
#ifdef NULL
    #undef NULL
    #define NULL nullptr
#endif // NULL

// Assertion
#ifdef NDEBUG
    #define assert(e) ((void)0)
    #define vassert(e) ((void)0)
#else
    #define assert(e) if (e) {} else {;__asm volatile("int3");}
    #ifdef VERBOSE
        #define vassert(e) assert(e)
    #else
        #define vassert(e) ((void)0)
    #endif
#endif

#define STATIC_ASSERT(expr) sizeof(int[(bool)(expr)?1:-1])

// Type info
template <typename T>
class __BITSOF_CLASS {
public:
    __BITSOF_CLASS() = delete;
    enum :int{
        NUM_BITS=sizeof(T)<<3
    };
};

template <>
class __BITSOF_CLASS<bool> {
public:
    __BITSOF_CLASS() = delete;
    enum :int{
        NUM_BITS=1
    };
};

// e.g. TYPE_BITS(int)=32 TYPE_BITS(bool)=1
#define TYPE_BITS(TP) (__BITSOF_CLASS<TP>::NUM_BITS)

// e.g. BIT_MASK(3)=7
// bits shouldn't be zero
#define BIT_MASK(TP,bits) (1|((((TP)1<<((bits)-1))-1)<<1))

// e.g. TYPE_MAX(int)=0xFFFFFFFF
#define TYPE_MAX(TP) BIT_MASK(TP,TYPE_BITS(TP))

#define COUNT_OF(arr) (sizeof(arr)/sizeof(arr[0]))

// Bit manipulation
#define LOW_BIT(x) ((x)&(-(x)))
#define RTRIM(x) ((x)/LOW_BIT(x))

// Type casting
#ifdef SAFE_CASTING
    #define FASTCAST(VAR,TP) (safe_cast<TP>(VAR))
    #define FASTCONSTCAST(VAR,TP) (safe_cast<const TP>(VAR))
#else
    #define FASTCAST(VAR,TP) ((TP&)*(TP*)(&VAR))
    #define FASTCONSTCAST(VAR,TP) ((const TP&)*(TP*)(&(VAR)))
#endif // COMPILE_ONLY
template <typename destType,typename srcType>
inline destType& safe_cast(srcType& source)
{
    static_assert(sizeof(srcType)==sizeof(destType),"Casting can only be performed between types of the same size");
    return *(destType*)(&source);
}
// Frequently used
template <typename T>
inline T min(const T& x,const T& y) {return x<y?x:y;}

template <typename T>
inline T max(const T& x,const T& y) {return x>y?x:y;}

template <typename T>
inline T max(const T& x,const T& y,const T& z) {return max(max(x,y),z);}