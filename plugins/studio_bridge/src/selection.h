#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/types/uuid.h"
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor's current selection set (entity ids the viewports highlight and the gizmo
    /// transforms). Owned by the bridge; written by the editor's view.setSelection and read by the
    /// gizmo + render overlay.
    /// @details
    /// Ownership: Value type owning the id list. Thread Safety: Main-thread only (set and read from
    /// on_update / request handling).
    class Selection
    {
      public:
        /// @brief Replaces the set from a { ids: [...] } params object (ignores malformed entries).
        void set_from_params(const tbx::Json& params)
        {
            auto ids = std::vector<tbx::Uuid>();
            if (const auto it = params.find("ids"); it != params.end() && it->is_array())
            {
                ids.reserve(it->size());
                for (const auto& value : *it)
                    if (value.is_number_unsigned())
                        ids.push_back(tbx::Uuid(value.get<uint32>()));
            }

            _ids = std::move(ids);
        }

        /// @brief The selected entity ids.
        const std::vector<tbx::Uuid>& ids() const { return _ids; }

        /// @brief Whether nothing is selected.
        bool empty() const { return _ids.empty(); }

      private:
        std::vector<tbx::Uuid> _ids = {};
    };
}
