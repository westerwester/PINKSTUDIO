#include "pink/mixer/Mixer.h"

#include <algorithm>
#include <cmath>

namespace pink
{
void MixerChannel::computePanGains (float panValue, bool isMono, float& gl, float& gr) noexcept
{
    const float p = std::clamp (panValue, -1.0f, 1.0f);
    if (isMono)
    {
        // constant power, -3 dB in the centre
        const float angle = (p + 1.0f) * (float) kPi * 0.25f;
        gl = std::cos (angle);
        gr = std::sin (angle);
    }
    else
    {
        // stereo balance: unity in the centre, the opposite side fades out
        gl = p <= 0.0f ? 1.0f : std::cos (p * (float) kPi * 0.5f);
        gr = p >= 0.0f ? 1.0f : std::cos (-p * (float) kPi * 0.5f);
    }
}

void MixerModel::setNumInsertChannels (int count)
{
    count = std::clamp (count, 1, kMaxInsertChannels);
    const int oldSize = (int) channels.size();
    channels.resize ((size_t) count + 1);

    for (int i = oldSize; i < (int) channels.size(); ++i)
    {
        auto& c = channels[(size_t) i];
        c.name = i == kMaster ? "Master" : "Insert " + std::to_string (i);
        c.output = i == kMaster ? -1 : kMaster;
    }

    const int size = (int) channels.size();
    for (auto& c : channels)
    {
        if (c.output >= size) c.output = kMaster;
        for (auto& s : c.sends)
            if (s.target >= size) s.target = -1;
        if (c.sidechainSource >= size) c.sidechainSource = -1;
    }
    channels[kMaster].output = -1;
}

std::vector<int> MixerModel::getDownstream (int index) const
{
    std::vector<int> out;
    if (! isValid (index))
        return out;

    const auto& c = channels[(size_t) index];
    if (c.output >= 0) out.push_back (c.output);
    for (const auto& s : c.sends)
        if (s.target >= 0) out.push_back (s.target);
    for (int i = 0; i < (int) channels.size(); ++i)
        if (channels[(size_t) i].sidechainSource == index)
            out.push_back (i);
    return out;
}

bool MixerModel::wouldCreateCycle (int from, int to) const
{
    if (! isValid (from) || ! isValid (to))
        return true;
    if (from == to)
        return true;

    // a cycle appears if 'from' is reachable from 'to'
    std::vector<char> visited (channels.size(), 0);
    std::vector<int> stack { to };
    while (! stack.empty())
    {
        const int n = stack.back();
        stack.pop_back();
        if (n == from)
            return true;
        if (visited[(size_t) n])
            continue;
        visited[(size_t) n] = 1;
        for (int d : getDownstream (n))
            stack.push_back (d);
    }
    return false;
}

bool MixerModel::setOutput (int index, int target)
{
    if (! isValid (index) || index == kMaster)
        return false;
    if (target == -1)
    {
        channels[(size_t) index].output = -1;
        return true;
    }
    if (! isValid (target) || wouldCreateCycle (index, target))
        return false;
    channels[(size_t) index].output = target;
    return true;
}

int MixerModel::addSend (int index, int target, float levelDb, bool preFader)
{
    if (! isValid (index) || ! isValid (target) || wouldCreateCycle (index, target))
        return -1;

    auto& c = channels[(size_t) index];
    int slot = -1;
    for (int i = 0; i < (int) c.sends.size(); ++i)
        if (c.sends[(size_t) i].target < 0) { slot = i; break; }

    if (slot < 0)
    {
        if ((int) c.sends.size() >= MixerChannel::kMaxSends)
            return -1;
        c.sends.push_back ({});
        slot = (int) c.sends.size() - 1;
    }

    c.sends[(size_t) slot] = { target, preFader };
    c.rt->sendLevelDb[(size_t) slot].store (levelDb);
    return slot;
}

bool MixerModel::removeSend (int index, int slot)
{
    if (! isValid (index))
        return false;
    auto& c = channels[(size_t) index];
    if (slot < 0 || slot >= (int) c.sends.size())
        return false;
    c.sends[(size_t) slot].target = -1; // slots stay stable so level atomics keep their meaning
    return true;
}

bool MixerModel::setSidechainSource (int index, int source)
{
    if (! isValid (index))
        return false;
    if (source == -1)
    {
        channels[(size_t) index].sidechainSource = -1;
        return true;
    }
    if (! isValid (source) || wouldCreateCycle (source, index))
        return false;
    channels[(size_t) index].sidechainSource = source;
    return true;
}

bool MixerModel::addInsert (int index, std::shared_ptr<Processor> processor)
{
    if (! isValid (index) || processor == nullptr)
        return false;
    auto& c = channels[(size_t) index];
    if ((int) c.inserts.size() >= MixerChannelModel::kMaxInserts)
        return false;
    c.inserts.push_back (std::move (processor));
    return true;
}

} // namespace pink
