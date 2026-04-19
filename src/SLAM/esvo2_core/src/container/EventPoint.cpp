// #include <esvo2_core/container/EventPoint.h>

// namespace esvo2_core
// {
// namespace container
// {
// EventPoint::EventPoint()
// {
//     row_ = 0;
//     col_ = 0;
//     ts_ = timePoint();
//     polarity_ = 0;
// }

// EventPoint::EventPoint(size_t row, size_t col)
// {
//     row_ = row;
//     col_ = col;
//     ts_ = timePoint();
//     polarity_ = 0;
// }

// EventPoint::EventPoint(size_t row, size_t col, timePoint &ts, uint8_t polarity)
// {
//     row_ = row;
//     col_ = col;
//     ts_ = ts;
//     polarity_ = polarity;
// }

// EventPoint::~EventPoint()
// {
// }

// size_t EventPoint::row() const
// {
//     return row_;
// }

// size_t EventPoint::col() const
// {
//     return col_;
// }

// timePoint EventPoint::ts() const
// {
//     return ts_;
// }

// uint8_t EventPoint::polarity() const
// {
//     return polarity_;
// }

// bool EventPoint::valid() const
// {
//     return esvo2_core::timePointToSec(ts_) > 0;
// }

// void EventPoint::copy(const EventPoint &copy)
// {
//     ts_ = copy.ts_;
//     polarity_ = copy.polarity_;
// }

// } // namespace container
// } // namespace esvo2_core