#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <deque>

// Undo / redo for everything the user does in Lumen's window: knob drags, switches, menus,
// presets and Reset. Each finished gesture is one step; up to `maxSteps` are kept.
//
// It listens to the parameters' change gestures, which only the plugin's own controls start, so
// host automation playback never fills the history. Snapshots are the parameters' normalised values.
class ParamHistory : private juce::AudioProcessorParameter::Listener,
                     public juce::ChangeBroadcaster
{
public:
    static constexpr size_t maxSteps = 100;

    explicit ParamHistory (juce::AudioProcessor& p) : processor (p)
    {
        for (auto* param : processor.getParameters())
            param->addListener (this);
    }

    ~ParamHistory() override
    {
        for (auto* param : processor.getParameters())
            param->removeListener (this);
    }

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    int numUndoSteps() const { return (int) undoStack.size(); }

    void undo() { step (undoStack, redoStack); }
    void redo() { step (redoStack, undoStack); }

    // Wrap a change made in code (preset, reset) so it becomes one undoable step.
    template <typename Fn>
    void performAsOneStep (Fn&& change)
    {
        const auto before = capture();
        {
            const juce::ScopedValueSetter<bool> quiet (suspended, true);
            change();
        }
        commit (before);
    }

    void clear()
    {
        undoStack.clear();
        redoStack.clear();
        sendChangeMessage();
    }

private:
    using Snapshot = std::vector<float>;

    Snapshot capture() const
    {
        Snapshot s;
        for (auto* param : processor.getParameters())
            s.push_back (param->getValue());
        return s;
    }

    void restore (const Snapshot& s)
    {
        const juce::ScopedValueSetter<bool> quiet (suspended, true);
        const auto& params = processor.getParameters();
        for (int i = 0; i < params.size() && i < (int) s.size(); ++i)
        {
            auto* param = params[i];
            if (std::abs (param->getValue() - s[(size_t) i]) > 1.0e-6f)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (s[(size_t) i]);
                param->endChangeGesture();
            }
        }
    }

    void commit (const Snapshot& before)
    {
        if (before == capture())
            return;
        undoStack.push_back (before);
        while (undoStack.size() > maxSteps)
            undoStack.pop_front();
        redoStack.clear();
        sendChangeMessage();
    }

    void step (std::deque<Snapshot>& from, std::deque<Snapshot>& to)
    {
        if (from.empty())
            return;
        to.push_back (capture());
        while (to.size() > maxSteps)
            to.pop_front();
        restore (from.back());
        from.pop_back();
        sendChangeMessage();
    }

    void parameterValueChanged (int, float) override {}

    void parameterGestureChanged (int, bool starting) override
    {
        // Only the plugin's own controls start gestures, and they do it on the message thread.
        if (suspended || ! juce::MessageManager::existsAndIsCurrentThread())
            return;

        if (starting)
        {
            if (openGestures++ == 0)
                pending = capture();
        }
        else if (openGestures > 0 && --openGestures == 0)
        {
            commit (pending);
        }
    }

    juce::AudioProcessor& processor;
    std::deque<Snapshot> undoStack, redoStack;
    Snapshot pending;
    int openGestures = 0;
    bool suspended = false;
};
