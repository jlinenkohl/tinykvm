#pragma once

#include <cstddef>
#include <type_traits>

namespace tinykvm {

template <typename T>
class Span {
public:
	using element_type = T;
	using value_type = typename std::remove_cv<T>::type;
	using pointer = T*;
	using reference = T&;
	using size_type = std::size_t;

	constexpr Span() noexcept = default;
	constexpr Span(pointer ptr, size_type count) noexcept
		: m_ptr(ptr), m_size(count) {}

	constexpr pointer data() const noexcept { return m_ptr; }
	constexpr size_type size() const noexcept { return m_size; }
	constexpr bool empty() const noexcept { return m_size == 0; }

	constexpr pointer begin() const noexcept { return m_ptr; }
	constexpr pointer end() const noexcept { return m_ptr + m_size; }

	constexpr reference operator[](size_type idx) const noexcept {
		return m_ptr[idx];
	}

private:
	pointer m_ptr = nullptr;
	size_type m_size = 0;
};

} // namespace tinykvm