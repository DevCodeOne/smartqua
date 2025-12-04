#pragma once

#include <memory>

template<template <typename...> typename SmartPointerType,
    typename Type,
    typename DestructorType,
    typename... Args>
SmartPointerType<Type, DestructorType> makePointerAt(void *destination, const DestructorType &destructor,
                                                   Args &&... args) {
    return {
        std::construct_at<Type>(static_cast<Type *>(destination), std::forward<Args>(args)...), destructor
    };
}
