// WPopupMenu's rows (0x432C60), shared by the war map's and the battle map's popups.
//
// Each item goes in the row whose first item has its command (+0x28; 0x432D70 / 0x432E90), else it starts a row
// (0x4331C0), at most 10 rows of 8. Laid out (0x433220) each row left to right from 0, or right to left from the
// menu's width when it was kept off the right edge (+0x1BC), the rows down by their first items' heights. Placed
// (0x432FB0) at the point less the first item's half size, kept 3 inside the screen (0x42F130; bit 8: off the right
// edge), laid out, then moved to the point less the first item's width / 2 and its place in the row, and its height
// / 4 (rounded toward 0), kept inside again; then every row closed to its first item (0x433330(-1)). An item under
// the mouse (0x80F1 with its command) opens its row and closes the rest.
// *Provisional*: the menu's own size (+0x34 / +0x38, for the edge test) is taken as the laid-out items' extent.
#pragma once
#include <algorithm>
#include <vector>

namespace wow
{
    // Item: { int command; Rect rect; int row, col; }. Items past the 10 rows of 8 are dropped.
    template <typename Item>
    void LayOutPopupRows(std::vector<Item>& popup, int x, int y, int screenW, int screenH)
    {
        if (popup.empty()) return;
        std::vector<int> rowCommand, rowCount;
        std::vector<Item> kept;
        for (Item item : popup)
        {
            int r = int(std::find(rowCommand.begin(), rowCommand.end(), item.command) - rowCommand.begin());
            if (r == int(rowCommand.size()))
            {
                if (r >= 10) continue;
                rowCommand.push_back(item.command);
                rowCount.push_back(0);
            }
            if (rowCount[size_t(r)] >= 8) continue;
            item.row = r;
            item.col = rowCount[size_t(r)]++;
            kept.push_back(item);
        }
        popup = std::move(kept);
        std::vector<int> rowW(rowCommand.size(), 0), rowH(rowCommand.size(), 0);
        for (const Item& item : popup)
        {
            rowW[size_t(item.row)] += item.rect.w;
            if (item.col == 0) rowH[size_t(item.row)] = item.rect.h;
        }
        int menuW = 0, menuH = 0;
        for (size_t r = 0; r < rowW.size(); r++) { menuW = std::max(menuW, rowW[r]); menuH += rowH[r]; }
        const auto first = popup[0].rect;
        auto keep = [&](int& mx, int& my, bool* right) {
            if (right) *right = mx + menuW > screenW - 3;
            mx = std::max(3, std::min(mx, screenW - 3 - menuW));
            my = std::max(3, std::min(my, screenH - 3 - menuH));
        };
        int mx = x - first.w / 2, my = y - first.h / 2;
        bool rtl = false;
        keep(mx, my, &rtl);
        std::vector<int> along(rowCommand.size(), rtl ? menuW : 0);
        std::vector<int> down(rowCommand.size(), 0);
        for (size_t r = 1; r < rowH.size(); r++) down[r] = down[r - 1] + rowH[r - 1];
        for (Item& item : popup)
        {
            int& a = along[size_t(item.row)];
            if (rtl) { a -= item.rect.w; item.rect.x = a; }
            else { item.rect.x = a; a += item.rect.w; }
            item.rect.y = down[size_t(item.row)];
        }
        mx = x - first.w / 2 - popup[0].rect.x;
        my = y - first.h / 4;
        keep(mx, my, nullptr);
        for (Item& item : popup) { item.rect.x += mx; item.rect.y += my; }
    }
}
