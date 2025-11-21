#pragma once

#include "./common.hpp"

#include <array>
#include <cinttypes>
#include <tuple>
#include <vector>

void * AllocBinds(size_t Count);
void * NextBind(void * BindPtr);
void * NextBindN(void * BindPtr, size_t N);
void   FreeBinds(void * BindPtr);

struct xMySqlTimestamp : xNonCopyable {
    static constexpr const size_t RESERVED_MYSQL_TIME_SIZE = 64;

    using xDummy = ::xel::xDummy<xMySqlTimestamp::RESERVED_MYSQL_TIME_SIZE>;

    void * GetDummyObjectAddress();
    size_t GetDummyObjectSize();
    void   ExtractTimestampFromDummyObject();
    void   ParseTimestampToDummyObject();

    xMySqlTimestamp();
    xMySqlTimestamp(int64_t Timestamp);
    ~xMySqlTimestamp();

    int64_t Timestamp = 0;
    xDummy  Dummy;
};

namespace __mysql_detail__ {

    int GetLongLongType();
    int GetFloatType();
    int GetDoubleType();
    int GetTimestampType();
    int GetStringType();
    int GetBinaryType();

}  // namespace __mysql_detail__

/****************
 *
 *
 */

namespace __mysql_detail__ {

    // clang-format off
	template <typename T, typename Inner =
		std::conditional_t<
			std::is_integral_v<std::remove_cv_t<T>>, long long,
		std::conditional_t<
			std::is_same_v<std::remove_cv_t<T>, float>, float,
		std::conditional_t<
			std::is_same_v<std::remove_cv_t<T>, double>, double,
		std::conditional_t<
			std::is_same_v<std::remove_cv_t<T>, xMySqlTimestamp>, xMySqlTimestamp,
		std::conditional_t<
			std::is_same_v<std::remove_cv_t<T>, std::string>, std::string,
		void>>>>>
	>
	struct __xInBind__;
    // clang-format on

    template <typename T>
    struct __xInBind__<T, void> {};

    template <typename T>
    struct __xInBind__<T, long long> {
        static_assert(std::is_integral_v<T>);
        __xInBind__(T & Referece) { Source = (long long)(Referece); }
        ~__xInBind__() {}
        long long Source;

        void *        GetSourcePtr() { return &Source; }
        unsigned long GetSourceSize() { return (unsigned long)sizeof(Source); }
        int           GetTypeEnum() const { return GetLongLongType(); }
    };

    template <typename T>
    struct __xInBind__<T, float> {
        static_assert(std::is_same_v<std::remove_cv_t<T>, float>);
        __xInBind__(T & value) { Source = value; }
        float Source;

        void *        GetSourcePtr() { return &Source; }
        unsigned long GetSourceSize() { return (unsigned long)sizeof(Source); }
        int           GetTypeEnum() const { return GetFloatType(); }
    };

    template <typename T>
    struct __xInBind__<T, double> {
        static_assert(std::is_same_v<std::remove_cv_t<T>, double>);
        __xInBind__(T & value) { Source = value; }
        ~__xInBind__() {}
        double Source;

        void *        GetSourcePtr() { return &Source; }
        unsigned long GetSourceSize() { return (unsigned long)sizeof(Source); }
        int           GetTypeEnum() const { return GetDoubleType(); }
    };

    template <typename T>
    struct __xInBind__<T, xMySqlTimestamp> {
        static_assert(std::is_same_v<std::remove_cv_t<T>, xMySqlTimestamp>);
        __xInBind__(T & value) { Source = &value; }
        ~__xInBind__() {}
        xMySqlTimestamp * Source;

        void *        GetSourcePtr() { return Source->GetDummyObjectAddress(); }
        unsigned long GetSourceSize() { return (unsigned long)Source->GetDummyObjectSize(); }
        int           GetTypeEnum() const { return GetTimestampType(); }
    };

    template <typename T>
    struct __xInBind__<T, std::string> {
        static_assert(std::is_same_v<std::remove_cv_t<T>, std::string>);
        __xInBind__(T & value) { Source = &value; }
        ~__xInBind__() {}
        const std::string * Source;

        void *        GetSourcePtr() { return (void *)Source->data(); }
        unsigned long GetSourceSize() { return (unsigned long)Source->size(); }
        int           GetTypeEnum() const { return GetStringType(); }
    };

    // TODO: Binary type:

}  // namespace __mysql_detail__

template <typename T>
struct xInBind
    : __mysql_detail__::__xInBind__<T>
    , xNonCopyable {
    using Base = __mysql_detail__::__xInBind__<T>;

    xInBind(T & Ref) : Base(Ref) {}
    xInBind(T && Ref) : Base(Ref) {}
};

/****************
 *
 *
 *
 */

namespace __mysql_detail__ {

    // clang-format off
	template <typename T, typename Inner =
		std::conditional_t<
			std::is_integral_v<T>, long long,
		std::conditional_t<
			std::is_same_v<T, float>, float,
		std::conditional_t<
			std::is_same_v<T, double>, double,
		std::conditional_t<
			std::is_same_v<T, xMySqlTimestamp>, xMySqlTimestamp,
		std::conditional_t<
			std::is_same_v<T, std::string>, std::string,
		std::conditional_t<
			std::is_same_v<T, std::vector<ubyte>>, std::vector<ubyte>,
		void>>>>>>
	>
	struct __xOutBind__;
    // clang-format on

    template <typename T>
    struct __xOutBind__<T, long long> {};

    // TODO: float
    // TODO: double

    template <typename T>
    struct __xOutBind__<T, xMySqlTimestamp> {
        __xOutBind__(T & Reference) { TargetPtr = &Reference; }
        ~__xOutBind__() {}

        void *        GetSourcePtr() { return TargetPtr->GetDummyObjectAddress(); }
        unsigned long GetSourceSize() { return TargetPtr->GetDummyObjectSize(); }
        int           GetTypeEnum() const { return GetTimestampType(); }

        xMySqlTimestamp * TargetPtr;
    };

    template <typename T>
    struct __xOutBind__<T, std::string> {
        static_assert(std::is_same_v<T, std::string>);
        __xOutBind__(T & Reference) { TargetPtr = &Reference; }
        ~__xOutBind__() {}
        std::string * TargetPtr;

        void * GetSourcePtr() {
            return nullptr;  // get length only
        }
        unsigned long GetSourceSize() { return 0; }
        int           GetTypeEnum() const { return GetStringType(); }
    };

    // TODO: binary

}  // namespace __mysql_detail__

struct xOutBind {

public:
    using xFetcher = bool (*)(xOutBind *, void * Stmt);

    template <typename T>
    std::enable_if_t<std::is_integral_v<T>> Setup(void * BindPoint, size_t Column, T & R) {
        SetupIntegral(BindPoint, &R, &FetchIntegral<T>);
        Reset(R);
    }
    // TODO:
    // void Setup(void * BindPoint, size_t Column, float & R);
    // TODO:
    // void Setup(void * BindPoint, size_t Column, double & R) ;
    void Setup(void * BindPoint, size_t Column, xMySqlTimestamp & Output);
    void Setup(void * BindPoint, size_t Column, std::string & Output);

    xFetcher GetFetcher() const { return Fetcher; }

protected:
    void SetupIntegral(void * BindPoint, void * OutputPtr, xFetcher Fetcher);

    void * TargetPtr;
    union {
        struct {
            void *        BindPoint;
            unsigned int  Column;
            unsigned long Length;  // receive length
        };
        long long LL;  // receive integral value
        float     F;   // not supported now
        double    D;   // not supported now
    } Receiver;
    xFetcher Fetcher;

    /* fetchers */
    template <typename T>
    static std::enable_if_t<std::is_integral_v<T>, bool> FetchIntegral(xOutBind * B, void *) {
        *(T *)B->TargetPtr = (T)B->Receiver.LL;
        return true;
    }
    // TODO: FetchFloat
    // TODO: FetchDouble
    static bool FetchTimestamp(xOutBind * B, void * Stmt);
    static bool FetchString(xOutBind * B, void * Stmt);

    // TODO: FetchBinary
};

template <size_t Offset, typename... tRArgs>
std::enable_if_t<(Offset < sizeof...(tRArgs))> SetupOutBindObject(std::array<xOutBind, sizeof...(tRArgs)> & Arr, void * BindPointStart, const std::tuple<tRArgs &...> & R) {
    auto & OutBind       = std::get<Offset>(Arr);
    auto & Output        = std::get<Offset>(R);
    auto   NodeBindPoint = NextBindN(BindPointStart, Offset);
    OutBind.Setup(NodeBindPoint, Offset, Output);
    SetupOutBindObject<Offset + 1>(Arr, BindPointStart, R);
}

template <size_t Offset, typename... tRArgs>
std::enable_if_t<(Offset == sizeof...(tRArgs))> SetupOutBindObject(std::array<xOutBind, sizeof...(tRArgs)> & Arr, void * BindPointStart, const std::tuple<tRArgs &...> & R) {
    // end of call chain
}

template <typename... tRArgs>
[[nodiscard]] std::array<xOutBind, sizeof...(tRArgs)> MakeOutBindIndirect(void * BindPointStart, const std::tuple<tRArgs &...> & R) {
    std::array<xOutBind, sizeof...(tRArgs)> Ret;
    SetupOutBindObject<0>(Ret, BindPointStart, R);
    return Ret;
}
