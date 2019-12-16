struct scope_guard {
    bool active_ = false;
    function<void(void)> f_;
    scope_guard() : f_() {}
    scope_guard(function<void(void)> f) : active_(true), f_(f) {}
    ~scope_guard() {
        if (active_)
            f_();
    }
    scope_guard(scope_guard const &) = delete;
    scope_guard &operator=(scope_guard const &) = delete;
    scope_guard(scope_guard &&) = delete;
    scope_guard &operator=(scope_guard &&p) {
        p.active_ = false;
        active_ = true;
        f_ = move(p.f_);
        return *this;
    }
};
