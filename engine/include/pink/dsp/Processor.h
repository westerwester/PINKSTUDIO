// PINKSTUDIO - base classes for every audio processor (stock effects, instruments and,
// in the app layer, hosted VST3 plug-ins wrapped behind the same interface).
#pragma once

#include "pink/core/AudioBuffer.h"
#include "pink/core/MidiEvent.h"
#include "pink/core/RtUtils.h"

#include <atomic>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace pink
{
class TempoMap;

struct ProcessContext
{
    double sampleRate = 48000.0;
    const TempoMap* tempo = nullptr;
    double ppqPosition = 0.0;
    double bpm = 120.0;
    bool isPlaying = false;
    /** Post-fader output of the channel selected as sidechain source (nullptr if none). */
    const AudioBuffer* sidechain = nullptr;
};

/** An automatable parameter. Value writes are lock-free; metadata is immutable after construction. */
class Parameter
{
public:
    enum class Scale { Linear, Logarithmic, Choice, Toggle };

    Parameter (std::string id, std::string name, float minValue, float maxValue, float defaultValue,
               std::string unit = {}, Scale scale = Scale::Linear, std::vector<std::string> choices = {})
        : paramId (std::move (id)), paramName (std::move (name)), unitLabel (std::move (unit)),
          choiceNames (std::move (choices)), minV (minValue), maxV (maxValue), defV (defaultValue),
          scaleType (scale), value (clampValue (defaultValue))
    {
    }

    const std::string& getId() const noexcept   { return paramId; }
    const std::string& getName() const noexcept { return paramName; }
    const std::string& getUnit() const noexcept { return unitLabel; }
    const std::vector<std::string>& getChoices() const noexcept { return choiceNames; }
    float getMin() const noexcept     { return minV; }
    float getMax() const noexcept     { return maxV; }
    float getDefault() const noexcept { return defV; }
    Scale getScale() const noexcept   { return scaleType; }

    float get() const noexcept { return value.load(); }
    int getInt() const noexcept { return (int) std::lround (get()); }
    bool getBool() const noexcept { return get() >= 0.5f; }
    void set (float v) noexcept { value.store (clampValue (v)); }
    void resetToDefault() noexcept { set (defV); }

    /** 0..1 mapping used by generic UIs, automation lanes and MIDI learn. */
    float getNormalised() const noexcept { return toNormalised (get()); }
    void setNormalised (float n) noexcept { set (fromNormalised (n)); }

    float toNormalised (float v) const noexcept
    {
        v = clampValue (v);
        if (maxV <= minV) return 0.0f;
        if (scaleType == Scale::Logarithmic && minV > 0.0f)
            return std::log (v / minV) / std::log (maxV / minV);
        return (v - minV) / (maxV - minV);
    }

    float fromNormalised (float n) const noexcept
    {
        n = std::clamp (n, 0.0f, 1.0f);
        if (scaleType == Scale::Logarithmic && minV > 0.0f)
            return minV * std::pow (maxV / minV, n);
        float v = minV + n * (maxV - minV);
        if (scaleType == Scale::Choice || scaleType == Scale::Toggle)
            v = std::round (v);
        return v;
    }

private:
    float clampValue (float v) const noexcept
    {
        if (! std::isfinite (v)) v = defV;
        v = std::clamp (v, minV, maxV);
        if (scaleType == Scale::Choice || scaleType == Scale::Toggle)
            v = std::round (v);
        return v;
    }

    std::string paramId, paramName, unitLabel;
    std::vector<std::string> choiceNames;
    float minV, maxV, defV;
    Scale scaleType;
    AtomicFloat value;
};

class ParameterSet
{
public:
    Parameter& add (std::string id, std::string name, float minValue, float maxValue, float defaultValue,
                    std::string unit = {}, Parameter::Scale scale = Parameter::Scale::Linear,
                    std::vector<std::string> choices = {})
    {
        params.push_back (std::make_unique<Parameter> (std::move (id), std::move (name), minValue, maxValue,
                                                       defaultValue, std::move (unit), scale, std::move (choices)));
        return *params.back();
    }

    Parameter& addChoice (std::string id, std::string name, std::vector<std::string> choices, int defaultIndex)
    {
        const float maxIndex = (float) std::max<size_t> (1, choices.size()) - 1.0f;
        return add (std::move (id), std::move (name), 0.0f, maxIndex, (float) defaultIndex, {}, Parameter::Scale::Choice, std::move (choices));
    }

    Parameter& addToggle (std::string id, std::string name, bool defaultOn)
    {
        return add (std::move (id), std::move (name), 0.0f, 1.0f, defaultOn ? 1.0f : 0.0f, {}, Parameter::Scale::Toggle);
    }

    Parameter* find (const std::string& id) const noexcept
    {
        for (auto& p : params)
            if (p->getId() == id)
                return p.get();
        return nullptr;
    }

    /** Convenience for tests and presets: returns false if the id does not exist. */
    bool set (const std::string& id, float value) const noexcept
    {
        if (auto* p = find (id)) { p->set (value); return true; }
        return false;
    }

    size_t size() const noexcept { return params.size(); }
    Parameter& operator[] (size_t i) const noexcept { return *params[i]; }

private:
    std::vector<std::unique_ptr<Parameter>> params;
};

class Processor
{
public:
    virtual ~Processor() = default;

    virtual std::string getName() const = 0;
    /** Message thread. May allocate. Called before the processor is used on the audio thread. */
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    /** Clears internal state (delay lines, envelopes). Real-time safe. */
    virtual void reset() noexcept = 0;
    /** Audio thread. In-place processing of io (stereo). */
    virtual void process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept = 0;

    virtual int getLatencySamples() const noexcept { return 0; }
    virtual bool acceptsSidechain() const noexcept { return false; }

    ParameterSet& getParameters() noexcept { return params; }
    const ParameterSet& getParameters() const noexcept { return params; }

    void setBypassed (bool b) noexcept { bypassed.store (b, std::memory_order_relaxed); }
    bool isBypassed() const noexcept { return bypassed.load (std::memory_order_relaxed); }

    /** Engine bookkeeping (message thread only): the prepare() generation this object has seen. */
    uint64_t enginePreparedGeneration = 0;

protected:
    ParameterSet params;

private:
    std::atomic<bool> bypassed { false };
};

/** A sound source driven by MIDI. Rendered by instrument tracks, not placed in insert chains. */
class Instrument : public Processor
{
public:
    /** Audio thread. Must overwrite (not add to) the first numFrames of out. */
    virtual void render (AudioBuffer& out, int numFrames, const MidiEventList& events, const ProcessContext& context) noexcept = 0;
    virtual int getActiveVoiceCount() const noexcept = 0;

    void process (AudioBuffer& io, int numFrames, const ProcessContext& context) noexcept override
    {
        static const MidiEventList none;
        render (io, numFrames, none, context);
    }
};

} // namespace pink
