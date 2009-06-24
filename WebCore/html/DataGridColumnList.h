#ifndef DataGridColumnList_h
#define DataGridColumnList_h

#include "DataGridColumn.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class AtomicString;

class DataGridColumnList : public RefCounted<DataGridColumnList> {
    friend class DataGridColumn;
public:
    static PassRefPtr<DataGridColumnList> create()
    {
        return new DataGridColumnList();
    }

    ~DataGridColumnList();

    unsigned length() const { return m_columns.size(); }

    DataGridColumn* item(unsigned index) const { return m_columns[index].get(); }
    DataGridColumn* itemWithName(const AtomicString&) const;

    DataGridColumn* primaryColumn() const { return m_primaryColumn.get(); }

    DataGridColumn* sortColumn() const { return m_sortColumn.get(); }

    DataGridColumn* add(const String& id, const String& label, const String& type, bool primary, unsigned short sortable);
    void remove(DataGridColumn*);
    void move(DataGridColumn*, unsigned long index);
    void clear();

private:
    void primaryColumnChanged(DataGridColumn* col);

    Vector<RefPtr<DataGridColumn> > m_columns;
    RefPtr<DataGridColumn> m_primaryColumn;
    RefPtr<DataGridColumn> m_sortColumn;
};

} // namespace WebCore

#endif // DataGridColumnList_h
