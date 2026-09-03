#ifndef MODEL_OBJECT_KIT_H
#define MODEL_OBJECT_KIT_H

#include <functional>
#include <utility>
#include <vector>

// 万能对象池：同一型号的多个业务封装类实例。
// 用户只需写「单个对象」的创建/初始化/步进/销毁逻辑，
// 工具在编译后的 Harness 中按 objectCount 自动复制并批量调用。
template<typename T>
class ModelObjPool {
public:
    using ValueType = T;
    using Pointer = T*;

    int size() const { return static_cast<int>(m_items.size()); }
    bool empty() const { return m_items.empty(); }

    Pointer at(int index) const {
        return m_items[static_cast<size_t>(index)];
    }

    Pointer operator[](int index) const { return at(index); }

    template<typename Factory>
    int CreateAll(int count, Factory&& factory) {
        Clear();
        if (count < 0) return -1;
        m_items.reserve(static_cast<size_t>(count));
        for (int index = 0; index < count; ++index) {
            Pointer created = factory(index);
            if (!created) return index;
            m_items.push_back(created);
        }
        return 0;
    }

    template<typename Fn>
    int ForEachInt(Fn&& fn) {
        for (int index = 0; index < size(); ++index) {
            const int rc = fn(m_items[static_cast<size_t>(index)], index);
            if (rc != 0) return rc;
        }
        return 0;
    }

    template<typename Fn>
    void ForEachVoid(Fn&& fn) {
        for (int index = 0; index < size(); ++index)
            fn(m_items[static_cast<size_t>(index)], index);
    }

    void Clear() {
        m_items.clear();
    }

    void ClearAndDelete(const std::function<void(Pointer, int)>& destroyer) {
        if (destroyer) {
            for (int index = 0; index < size(); ++index)
                destroyer(m_items[static_cast<size_t>(index)], index);
        }
        Clear();
    }

private:
    std::vector<Pointer> m_items;
};

// 对池中全部对象执行同一条语句。语句里用 obj 表示当前对象，objectId 表示下标。
// 示例：MODEL_CALL_ALL(g_models, obj->Initialize(objectId, R.lat, dt));
#define MODEL_CALL_ALL(pool, call) \
    (pool).ForEachInt([&](auto* obj, int objectId) -> int { \
        call; \
        return 0; \
    })

// 按数量复制「创建一个对象」的表达式。第一个参数须为整数常量或变量，不能写 objectCount（界面对象数在 Mo* 函数内不可见）。
#define MODEL_CREATE_ALL(pool, count, factory_body) \
    (pool).CreateAll((count), [&](int objectId) -> decltype(factory_body) { \
        return factory_body; \
    })

#endif // MODEL_OBJECT_KIT_H
