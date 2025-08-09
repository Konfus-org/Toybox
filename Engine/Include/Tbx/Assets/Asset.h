//#pragma once
//#include "Tbx/Debug/Debugging.h"
//#include <memory>
//#include <typeindex>
//#include <typeinfo>
//#include <stdexcept>
//
//namespace Tbx
//{
//    /// <summary>
//    /// A generic asset wrapper capable of holding any type of data using shared ownership.
//    /// Supports runtime type checking and safe retrieval of typed data.
//    /// </summary>
//    class Asset
//    {
//    public:
//        /// <summary>
//        /// Default constructor. Creates an empty asset.
//        /// </summary>
//        Asset() = default;
//
//        /// <summary>
//        /// Constructs an asset from a typed shared pointer.
//        /// </summary>
//        /// <typeparam name="T">The type of the data to store.</typeparam>
//        /// <param name="data">The shared pointer to the data.</param>
//        template<typename T>
//        Asset(std::shared_ptr<T> data)
//            : _data(std::static_pointer_cast<void>(data))
//            , _type(typeid(T))
//        {
//        }
//
//        /// <summary>
//        /// Sets the asset data to a new shared pointer of type T.
//        /// </summary>
//        /// <typeparam name="T">The type of the data to store.</typeparam>
//        /// <param name="data">The shared pointer to the data.</param>
//        template<typename T>
//        void Set(std::shared_ptr<T> data)
//        {
//            _data = std::static_pointer_cast<void>(data);
//            _type = typeid(T);
//        }
//
//        /// <summary>
//        /// Retrieves the stored data as a shared pointer of type T.
//        /// Throws std::bad_cast if the stored type does not match.
//        /// </summary>
//        /// <typeparam name="T">The expected type of the stored data.</typeparam>
//        /// <returns>A shared pointer to the data.</returns>
//        template<typename T>
//        std::shared_ptr<T> Get() const
//        {
//            if (_type != typeid(T))
//            {
//                TBX_ASSERT(false, "Stored type does not match requested type.");
//            }
//            return std::static_pointer_cast<T>(_data);
//        }
//
//        /// <summary>
//        /// Attempts to retrieve the stored data as a shared pointer of type T.
//        /// Returns nullptr if the stored type does not match.
//        /// </summary>
//        /// <typeparam name="T">The expected type of the stored data.</typeparam>
//        /// <returns>A shared pointer to the data, or nullptr.</returns>
//        template<typename T>
//        std::shared_ptr<T> TryGet() const
//        {
//            return (_type == typeid(T)) ? std::static_pointer_cast<T>(_data) : nullptr;
//        }
//
//        /// <summary>
//        /// Checks whether the asset currently holds any data.
//        /// </summary>
//        /// <returns>True if data is present; otherwise, false.</returns>
//        bool HasData() const
//        {
//            return _data != nullptr;
//        }
//
//        /// <summary>
//        /// Checks if the stored data is of the specified type.
//        /// </summary>
//        /// <typeparam name="T">The type to check against.</typeparam>
//        /// <returns>True if the stored data is of type T; otherwise, false.</returns>
//        template<typename T>
//        bool IsType() const
//        {
//            return _type == typeid(T);
//        }
//
//        /// <summary>
//        /// Gets the type_info of the stored data.
//        /// </summary>
//        /// <returns>The std::type_index of the stored type.</returns>
//        std::type_index GetType() const
//        {
//            return _type;
//        }
//
//        /// <summary>
//        /// Gets the type_info of the stored data.
//        /// </summary>
//        /// <returns>The std::type_index of the stored type.</returns>
//        bool IsValid() const
//        {
//            return _data != nullptr;
//        }
//
//    private:
//        std::shared_ptr<void> _data;
//        std::type_index _type = typeid(void);
//    };
//}
