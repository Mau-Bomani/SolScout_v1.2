#pragma once
#include <string>
#include <memory>
#include <sw/redis++/redis++.h>

class MuteState {
public:
    explicit MuteState(std::shared_ptr<sw::redis::Redis> redis);
    
    void set_mute(int64_t user_id, int minutes);
    void clear_mute(int64_t user_id);
    bool is_muted(int64_t user_id);
    int get_remaining_minutes(int64_t user_id);
    
private:
    std::shared_ptr<sw::redis::Redis> redis_;
    std::string mute_key(int64_t user_id) const;
};
