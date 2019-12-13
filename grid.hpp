class grid {
    vector<vector<char>> layers_;
    class row_ref {
        vector<char> &row_;

      public:
        template <class R>
        row_ref(R &row) : row_(const_cast<decltype(row_)>(row)) {}
        operator decltype(row_) &() { return row_; }
        auto &operator[](sig j) { return row_[static_cast<size_t>(j)]; }
        auto &operator[](sig j) const { return row_[static_cast<size_t>(j)]; }
    };
    class grid_iterator {
        plane_coord pos_;
        sig max_;
        bool ended_ = false;
        bool end_soon_ = false;

      public:
        grid_iterator(sig x, sig y, sig max_xy)
            : pos_(x, y, max_xy, 0), max_(max_xy) {}
        auto operator++() {
            if (pos_.x() == max_)
                pos_.x() = 0, ++pos_.y();
            else
                ++pos_.x();
            if (end_soon_)
                ended_ = true;
            if (pos_.x() == max_ && pos_.y() == max_)
                end_soon_ = true;
            return *this;
        }
        auto &operator*() { return pos_; }
        auto operator!=(bool ended) const { return ended_ != ended; }
    };

  public:
    grid(decltype(layers_) layers) : layers_(layers) {}
    auto operator[](plane_coord const &p) const {
        return layers_.at(size_t(p.y())).at(size_t(p.x()));
    }
    auto &operator[](plane_coord const &p) {
        return layers_.at(size_t(p.y())).at(size_t(p.x()));
    }
    auto operator[](pair<sig, sig> p) const {
        return operator[](plane_coord(p.first, p.second, size() - 1, 0));
    }
    auto &operator[](pair<sig, sig> p) {
        return operator[](plane_coord(p.first, p.second, size() - 1, 0));
    }
    auto begin() const { return grid_iterator(0, 0, sig(layers_.size()) - 1); };
    auto end() const { return true; }
    sig size() const { return static_cast<sig>(layers_.size()); }
};
using layers = vector<grid>;
