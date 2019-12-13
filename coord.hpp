class limited_val {
    sig min_, max_;
    sig val_;

  public:
    limited_val(sig val, sig max, sig min)
        : min_(min), max_(max), val_(clamp(val, min_, max_)) {}
    operator sig() const { return val_; }
    auto operator=(sig x) {
        val_ = x;
        return *this;
    }
    auto operator++() {
        if (val_ < max_)
            val_++;
        return *this;
    }
    auto operator--() {
        if (val_ > min_)
            val_--;
        return *this;
    }
    auto operator+(sig p) const {
        if (val_ + p <= max_)
            return limited_val{val_ + p, max_, min_};
        else
            return *this;
    }
    auto operator-(sig p) {
        if (val_ - p >= min_)
            return limited_val{val_ - p, max_, min_};
        else
            return *this;
    }
};

class plane_coord {
    sig min_, max_;
    limited_val x_, y_;

  public:
    plane_coord(sig x, sig y, sig max_xy, sig min_xy)
        : min_(min_xy), max_(max_xy), x_(x, max_, min_), y_(y, max_, min_) {}
    auto &x() { return x_; }
    auto &x() const { return x_; }
    auto &y() { return y_; }
    auto &y() const { return y_; }
    bool operator==(plane_coord const &p) const {
        return x_ == p.x_ && y_ == p.y_;
    }
    bool operator!=(plane_coord const &p) const { return !(operator==(p)); }
    bool operator<(plane_coord const &p) const {
        if (x() != p.x())
            return x() < p.x();
        else
            return y() < p.y();
    }
};
ostream &operator<<(ostream &o, plane_coord p) {
    o << "(" << p.x() << "," << p.y() << ")";
    return o;
}

enum class side : sig { LEFT = 0, RIGHT = 1, UP = 2, DOWN = 3 };
side next_side(side p) {
    return static_cast<side>((static_cast<sig>(p) + 1) % 4);
}

class wall_coord {
    sig max_;
    limited_val u_;
    side side_;

  public:
    wall_coord(side side, sig u, sig max_u)
        : max_(max_u), u_(u, max_, 0), side_(side) {}
    auto &u() { return u_; }
    auto &u() const { return u_; }
    auto side() const { return side_; }
    bool operator==(wall_coord const &p) const {
        return side_ == p.side_ && u_ == p.u_;
    }

    plane_coord to_plane(sig max_xy, sig min_xy) const {
        switch (side_) {
        case side::LEFT:
            return {0, u_, max_xy, min_xy};
        case side::RIGHT:
            return {max_xy, u_, max_xy, min_xy};
        case side::UP:
            return {u_, 0, max_xy, min_xy};
        case side::DOWN:
            return {u_, max_xy, max_xy, min_xy};
        }
    }
};
ostream &operator<<(ostream &o, wall_coord p) {
    char c = 0;
    switch (p.side()) {
    case side::LEFT:
        c = 'L';
        break;
    case side::RIGHT:
        c = 'R';
        break;
    case side::UP:
        c = 'U';
        break;
    case side::DOWN:
        c = 'D';
        break;
    }
    o << "(" << c << "," << p.u() << ")";
    return o;
}
