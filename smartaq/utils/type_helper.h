#pragma once

template<typename T>
concept IsByteCopyable = std::is_standard_layout_v<T> && std::is_trivial_v<T>;