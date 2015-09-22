#ifndef _FBS_TK_HPP_
#define _FBS_TK_HPP_

#include <flatbuffers/idl.h>
#include <flatbuffers/util.h>
#include <iostream>
#include <cstring>
#include <iterator>
#include <cassert>

/**
 * Add equality to flatbuffers::String
 */
namespace flatbuffers{
inline bool string_eq(const flatbuffers::String &str1, const flatbuffers::String &str2) {
	return str1.str() == str2.str();
}
inline bool operator==(const flatbuffers::String &str1, const flatbuffers::String &str2) {
	return string_eq(str1, str2);
}
inline bool operator!=(const flatbuffers::String &str1, const flatbuffers::String &str2) {
	return !string_eq(str1, str2);
}
}


namespace fbs_tk {
// http://stackoverflow.com/questions/13037490/
template <typename T>
struct range
{
    range(std::istream& in): d_in(in) {}
    std::istream& d_in;
};

template <typename T>
std::istream_iterator<T> begin(fbs_tk::range<T> r) {
    return std::istream_iterator<T>(r.d_in);
}

template <typename T>
std::istream_iterator<T> end(fbs_tk::range<T>) {
    return std::istream_iterator<T>();
}

// Loads all the contents of the input stream into a string
inline bool load_buffer(std::istream &in, std::string *buf) {
  *buf = std::string(std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
  return !in.bad();
}

// Copies a buffer into a string

inline void buffer_copy(const void *data, size_t size, std::string &dst) {
	dst.resize(size);
	memcpy(&dst[0], data, size);
}

struct Buffer {
	Buffer() : data() {}
	
	Buffer(std::initializer_list<uint8_t> args) : data(args) {}
	
	Buffer(std::vector<uint8_t> data) : data(std::move(data)) {}
	
	Buffer(const std::string &str) : data(str.begin(), str.end()) {}
	
	Buffer(const Buffer &buff) : Buffer(buff.data) {}

	inline void copy_from(const void *buff, size_t buff_size) {
		data.resize(buff_size);
		memcpy(data.data(), buff, buff_size);
	}

	inline void copy_from(const std::string &buff) {
		copy_from(buff.data(), buff.size());
	}
	
	inline void copy_to(void *buff) const {
		memcpy(buff, data.data(), data.size());
	}

	inline std::vector<uint8_t> &get_data() {
		return data;
	}

	inline const std::vector<uint8_t> &get_data() const {
		return data;
	}

	inline std::string str() const {
		return std::string(data.begin(), data.end());
	}
	
	inline bool operator==(const Buffer &other) const {
		return data == other.data;
	}
	
	inline size_t size() const {
		return data.size();
	}
	
	/**
	 * Writes all data in this buffer into the output stream.
	 */
	inline void write_data(std::ostream &out) const {
		out.write(reinterpret_cast<const char*>(data.data()), data.size());
	}
	
	/**
	 * Clears the data in the current buffer and then reads a certain number
	 * of bytes into this buffer.
	 */
	inline void read_data(std::istream &in, size_t size) {
		data.clear();
		data.resize(size);
		in.read(reinterpret_cast<char *>(data.data()), size);
	}

	/**
	 * Loads all data available in the input stream into this buffer,
	 * replacing the contents of this buffer.
	 */
	inline std::istream & read_all_data(std::istream &in) {
		data.clear();
		// make sure we do not skip precious bytes
		// http://stackoverflow.com/questions/8075795/
		in >> std::noskipws;
		data.assign(std::istream_iterator<char>(in), std::istream_iterator<char>());
		return in;
	}

	
private:
	std::vector<uint8_t> data;

	friend std::istream &operator>>(std::istream &in, Buffer &buff) {
		char size_buff[sizeof(uint32_t)];
		in.read(size_buff, sizeof(size_buff));
		if (! in) {
			return in;
		}
		auto size = flatbuffers::ReadScalar<uint32_t>(size_buff);
		buff.read_data(in, size);
	  	return in;
    }
    
    friend std::ostream &operator<<(std::ostream &out, const Buffer &buff) {
		uint32_t size = buff.size();
		char size_buff[sizeof(uint32_t)];
		flatbuffers::WriteScalar(size_buff, size);
		out.write(size_buff, sizeof(uint32_t));
		buff.write_data(out);
		return out;
    }
};

inline void copy_from(Buffer &buffer, const flatbuffers::FlatBufferBuilder &builder) {
	buffer.copy_from(builder.GetBufferPointer(), builder.GetSize());
}


// converts a json object into a binary FBS
bool json_to_bin(flatbuffers::Parser &parser, const char *js, Buffer &bin);
// converts a binary FBS into a json object
std::string bin_to_json(flatbuffers::Parser &parser, const Buffer &bin);

// converts a json object into a binary FBS
bool json_to_fbs(flatbuffers::Parser &parser, std::istream &in, std::ostream &out);
// converts a binary FBS into a json object
bool fbs_to_json(flatbuffers::Parser &parser, std::istream &in, std::ostream &out);

// converts a stream of binary FBS into a stream of JSON objects
bool fbs_stream_to_jsonl(const std::string &schema, std::istream &in, std::ostream &out);
// converts a stream of JSON objects into a stream of binary FBS
bool jsonl_to_fbs_stream(const std::string &schema, std::istream &in, std::ostream &out);

// GetRoot from a string
template<class T>
inline const T* get_root(const Buffer &buff) {
	if (buff.size() == 0) {
		// when the buffer is empty, buff.get_data().data() might not
		// be initialized
		return nullptr;
	}
	auto data = buff.get_data().data();
	auto check = flatbuffers::Verifier(data, buff.size());
	return check.VerifyBuffer<T>() ? flatbuffers::GetRoot<T>(data) : nullptr;
}

template<class T>
inline const T* get_root_unsafe(const Buffer &buff) {
	return flatbuffers::GetRoot<T>(buff.get_data().data());
}

 // Copy a FBS object.
template<typename T>
struct copy {
	flatbuffers::Offset<T> operator()(flatbuffers::FlatBufferBuilder &, const T&) const;
};

// Create a root object and bundle it with its data
template <class T>
struct Root {
	Root() : root(nullptr), data() {}
	
	Root(Buffer buff) : data(std::move(buff)) {
		update_root();
	}

	Root(const Root &other) : Root(other.data) {}

	/**
	 * Finishes the builder with <code>obj</code> as root and
	 * copies the data from the builder.
	 */
	Root(flatbuffers::FlatBufferBuilder &builder, flatbuffers::Offset<T> obj) : data() {
		builder.Finish(obj);
		copy_from(data, builder);
		update_root_unsafe();
	}

	const T* operator->() const {
		assert(valid());
		return root;
	}

	const T& operator*() const {
		assert(valid());
		return *root;
	}
	
	bool operator==(const Root<T> &other) const {
		assert(valid());
		return *root == *other.root;
	}

	bool operator!=(const Root<T> &other) const {
		assert(valid());
		return *root != *other.root;
	}
	
	bool valid() const {
		return root != nullptr;
	}
	
	const Buffer & get_data() const {
		assert(valid());
		return data;
	}

	void set_data(Buffer buff) {
		data = std::move(buff);
		update_root();
	}
	
private:
	inline void update_root() {
		root = get_root<T>(data);
	}

	inline void update_root_unsafe() {
		root = get_root_unsafe<T>(data);
	}
	
	friend std::istream &operator>>(std::istream &in, Root<T> &root) {
		in >> root.data;
		root.update_root();
		if (!root.valid()) {
			// mark the input stream as invalid
			in.setstate(in.badbit);
		}
		return in;
	}

	friend std::ostream &operator<<(std::ostream &out, const Root<T> &root) {
		assert(root.valid());
		out << root.data;
		return out;
	}

    const T* root;
	Buffer data;
};

namespace root {
	/**
	 * Copies a FBS object as a root object.
	 */
	template<class S>
	static inline Root<S> copy(const S& other) {
		flatbuffers::FlatBufferBuilder b;
		return Root<S>(b, fbs_tk::copy<S>()(b, other));
	}
	
	/**
	 * Copies a root object as a shared_copy of a root object.
	 */
	template<class S>
	static inline std::shared_ptr<fbs_tk::Root<S>> copy_shared(const Root<S> &obj) {
		return std::make_shared<Root<S>>(obj);
	}
	
	/**
	 * Copies a FBS object as a shared root object.
	 */
	template<class S>
	static inline std::shared_ptr<fbs_tk::Root<S>> copy_shared(const S &obj) {
		return root::copy_shared<S>(root::copy(obj));
	}
}

template<class T>
Root<T> open_root(std::string filename) {
	std::ifstream ifs(filename, std::ifstream::binary);
	if (!ifs.is_open()) {
		return Root<T>();
	}
	Buffer buff;
	buff.read_all_data(ifs);
	fbs_tk::Root<T> result(buff);
	if (ifs.bad()) {
		ifs.close();
		return Root<T>();
	}
	ifs.close();
	return result;
}

} // namespace

// Define hash code for Root objects
namespace std {
template<class T>
struct hash<fbs_tk::Root<T>> {
	std::size_t operator()(fbs_tk::Root<T> const& obj) const {
		return std::hash<T>()(*obj);
	}
};
}
#endif // _FBS_TK_HPP_
