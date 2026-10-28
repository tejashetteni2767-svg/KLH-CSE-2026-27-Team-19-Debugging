/**
 * ============================================================================
 * Project: Linux Signal-Based Process Controller (OSSP Hackathon)
 * File: web/js/presentation.js
 * Description: Interactive Slide Deck Manager & Speaker Notes Controller
 * ============================================================================
 */

(function () {
  'use strict';

  let currentSlide = 1;
  const totalSlides = 10;

  // Speaker notes per slide tailored for faculty presentation
  const SPEAKER_NOTES = {
    1: `<strong>Faculty Pitch:</strong> Welcome the evaluators. Highlight that this is not merely a front-end mockup—it is backed by native POSIX C system calls and direct Linux procfs kernel introspection. Mention our dual-mode architecture: zero-install simulation + live WSL2 Linux execution.`,
    2: `<strong>Theoretical Basis:</strong> Emphasize that signals are the oldest, most lightweight form of asynchronous IPC in Unix. Explain the kernel lifecycle: signal generation -> pending state in task_struct -> delivery on context switch back to user space.`,
    3: `<strong>Taxonomy Focus:</strong> Point out the critical columns in the table: whether signals are catchable and blockable. Contrast standard signals (1-31, bitmask merge) with POSIX real-time signals.`,
    4: `<strong>Core Viva Question:</strong> Professors LOVE asking why SIGSTOP cannot be caught. Answer clearly: "If user processes could intercept SIGSTOP and SIGKILL, a malicious or stuck process could become unkillable and permanently monopolize CPU or system resources. The kernel enforces this rule in kernel space."`,
    5: `<strong>State Transitions:</strong> Walk through the state machine. Explain why a stopped process has state 'T' in /proc. Clarify that 'S' (interruptible sleep) awakens on signals, whereas 'D' (uninterruptible sleep) waits on device drivers.`,
    6: `<strong>C Implementation:</strong> Emphasize that we used modern sigaction() with SA_SIGINFO rather than legacy signal(), preventing race conditions. Point out that sigaction gives us the sender's PID and UID.`,
    7: `<strong>ProcFS Introspection:</strong> Walk through the bitmask decoding algorithm. Explain that each bit represents (1 << (signo - 1)). Mention how SigCgt: 0x4a03 decodes mathematically to SIGHUP (1), SIGINT (2), SIGUSR1 (10), SIGUSR2 (12), SIGTERM (15). Evaluators love this technical rigor!`,
    8: `<strong>Zombie Processes:</strong> Explain that a zombie retains zero RAM but consumes an entry in the kernel process table. Highlight our solution: asynchronous SIGCHLD handling using non-blocking waitpid(-1, &status, WNOHANG).`,
    9: `<strong>Real-World Relevance:</strong> Tie our project directly to Docker and Kubernetes container lifecycles. Mention terminationGracePeriodSeconds (SIGTERM followed by SIGKILL fallback), and Nginx SIGHUP config reloading.`,
    10: `<strong>Viva Wrap-up:</strong> Review the cheat sheet. Encourage faculty to ask questions or test live experiments in Tab 3 (Interactive Signal Lab)!`
  };

  function initPresentation() {
    const prevBtn = document.getElementById('prevSlideBtn');
    const nextBtn = document.getElementById('nextSlideBtn');
    const fullscreenBtn = document.getElementById('fullscreenPresBtn');
    const toggleNotesBtn = document.getElementById('toggleNotesBtn');
    const closeNotesBtn = document.getElementById('closeNotesBtn');
    const notesDrawer = document.getElementById('speakerNotesDrawer');

    if (prevBtn) prevBtn.addEventListener('click', prevSlide);
    if (nextBtn) nextBtn.addEventListener('click', nextSlide);
    if (fullscreenBtn) fullscreenBtn.addEventListener('click', toggleFullscreen);

    if (toggleNotesBtn) {
      toggleNotesBtn.addEventListener('click', () => {
        notesDrawer.classList.toggle('hidden');
        updateSpeakerNotes();
      });
    }

    if (closeNotesBtn) {
      closeNotesBtn.addEventListener('click', () => {
        notesDrawer.classList.add('hidden');
      });
    }

    // Keyboard Shortcuts
    document.addEventListener('keydown', (e) => {
      // Only when presentation tab is active
      const presTab = document.getElementById('tab-presentation');
      if (!presTab || !presTab.classList.contains('active')) return;

      if (e.key === 'ArrowRight' || e.key === ' ') {
        e.preventDefault();
        nextSlide();
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        prevSlide();
      } else if (e.key === 'f' || e.key === 'F') {
        if (!e.target.matches('input, textarea, select')) {
          e.preventDefault();
          toggleFullscreen();
        }
      }
    });

    showSlide(1);
  }

  function showSlide(slideNum) {
    if (slideNum < 1) slideNum = 1;
    if (slideNum > totalSlides) slideNum = totalSlides;

    currentSlide = slideNum;

    const slides = document.querySelectorAll('.slide');
    slides.forEach((slide) => {
      const num = parseInt(slide.getAttribute('data-slide'), 10);
      if (num === currentSlide) {
        slide.classList.add('active');
      } else {
        slide.classList.remove('active');
      }
    });

    // Update Counter
    const counter = document.getElementById('slideCounter');
    if (counter) {
      counter.textContent = `Slide ${currentSlide} of ${totalSlides}`;
    }

    // Update Prev/Next button disabled state
    const prevBtn = document.getElementById('prevSlideBtn');
    const nextBtn = document.getElementById('nextSlideBtn');
    if (prevBtn) prevBtn.disabled = currentSlide === 1;
    if (nextBtn) nextBtn.disabled = currentSlide === totalSlides;

    updateSpeakerNotes();
  }

  function nextSlide() {
    if (currentSlide < totalSlides) {
      showSlide(currentSlide + 1);
    }
  }

  function prevSlide() {
    if (currentSlide > 1) {
      showSlide(currentSlide - 1);
    }
  }

  function updateSpeakerNotes() {
    const notesContent = document.getElementById('speakerNotesContent');
    if (notesContent) {
      notesContent.innerHTML = SPEAKER_NOTES[currentSlide] || 'No notes for this slide.';
    }
  }

  function toggleFullscreen() {
    const elem = document.getElementById('slideDeckViewport');
    if (!elem) return;

    if (!document.fullscreenElement) {
      elem.requestFullscreen().catch((err) => {
        console.warn('Fullscreen request failed:', err);
      });
    } else {
      document.exitFullscreen();
    }
  }

  window.PresentationManager = {
    init: initPresentation,
    showSlide: showSlide,
    nextSlide: nextSlide,
    prevSlide: prevSlide
  };
})();
