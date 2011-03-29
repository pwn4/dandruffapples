#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <typeinfo>

// Variant that holds anything
// http://drdobbs.com/cpp/184401293, Fernando Cacciola
class variant {
public:
	variant() : data(NULL) {}
	variant(const variant & rhs) {
		if (rhs.data != NULL)
			rhs.data->AddRef();
		data = rhs.data;
	}
	~variant() {
		if (data != NULL)
			data->Release();
	}
	variant& operator = (const variant& rhs) {
		if (rhs.data != NULL)
			rhs.data->AddRef();
		if (data != NULL)
			data->Release();
		data = rhs.data;
		return * this;
	}

	template<typename T> variant(T v) : data (new Impl<T>(v)) {
		data->AddRef();
	}

	template<typename T> operator T () const {
		return CastFromBase<T>(data)->data;
	}

	template<typename T> const T & get() const {
		return CastFromBase<T>(data)->data;
	}

	template<typename T> bool is_type() const {
		return typeid(*data) == typeid(Impl<T>);
	}

	template<typename T> bool is_type(T v) const {
		return typeid(*data) == typeid(v);
	}
private:
	struct ImplBase {
		ImplBase() : refs(0) {}
		virtual ~ImplBase() {}
		void AddRef() { refs ++; }
		void Release() { refs --;
			if (refs == 0) delete this;
		}
		size_t refs;
	};

	template<typename T>
	struct Impl : ImplBase {
		Impl(T v) : data(v) {}
		~Impl() {}
		T data;
	};

	template<typename T>
	static Impl<T>* CastFromBase(ImplBase* v) {
		Impl<T>* p = dynamic_cast<Impl<T>*>(v);
		if (p == NULL)
			throw invalid_argument(typeid(T).name()+string(" is not a valid type"));
		return p;
	}

	ImplBase* data;
};

#endif
