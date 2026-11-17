"""Fail-closed checks that must pass before a new-round training run."""

import argparse
import csv
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REQUIRED_PROPOSAL_FIELDS = {
    "id",
    "phase",
    "training_stage",
    "type",
    "observation",
    "primary_hypothesis",
    "question_inherited",
    "unresolved_from_reference",
    "why_this_experiment",
    "modification",
    "changed_components",
    "held_constant_components",
    "controlled_variables",
    "expected_evidence",
    "rejection_condition",
    "minimum_evidence_requirements",
    "required_diagnostics",
    "estimated_budget",
    "model",
    "reference_experiment",
}
EXPERIMENT_TYPES = {
    "baseline",
    "architecture",
    "loss",
    "compression",
    "physics",
    "optimization",
    "ablation",
    "combined",
    "compression",
    "distillation",
    "quantization",
    "tinyml",
}
VALID_PHASES = {"phase1_model", "phase2_loss", "phase3_tinyml"}
VALID_TRAINING_STAGES = {"smoke", "screening", "full_candidate", "confirmation"}
REQUIRED_LOCK_FIELDS = {
    "protocol_version",
    "approved_at",
    "protocol_fingerprint",
    "approved_protocol_sha256",
    "cache_path",
    "cache_sha256",
    "data_implementation_sha256",
    "metric_implementation_sha256",
    "accepted_cell_ids_sha256",
    "split_membership_sha256",
}


def load_json(name):
    with (ROOT / name).open() as handle:
        return json.load(handle)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def recorded_ids():
    results_path = ROOT / "results.tsv"
    if not results_path.exists():
        return set()
    with results_path.open(newline="") as handle:
        return {
            row["experiment"]
            for row in csv.DictReader(handle, delimiter="\t")
            if row.get("experiment")
        }


def result_rows():
    results_path = ROOT / "results.tsv"
    if not results_path.exists():
        return []
    with results_path.open(newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def validate(experiment_id, proposal_path):
    errors = []
    state = load_json("state.json")
    lock = load_json("protocol_lock.json")
    budget = state.get("budget", {})

    if state.get("workflow_version") != "three-phase-relative-huber-v1":
        errors.append("state workflow version is not three-phase-relative-huber-v1")
    PHASE_MODES = {
        "phase1_model": "model_search_no_physics",
        "phase2_loss": "loss_search_physics",
        "phase3_tinyml": "tinyml_compression",
    }
    if state.get("phase") not in PHASE_MODES:
        errors.append("state.phase must be phase1_model, phase2_loss, or phase3_tinyml")
    elif state.get("research_mode") != PHASE_MODES[state.get("phase")]:
        errors.append("research mode does not match the active phase")
    if state.get("protocol_status") != "locked":
        errors.append("state protocol status is not locked")
    if not state.get("run_approved"):
        errors.append("run gate is closed")
    if lock.get("status") != "locked":
        errors.append("protocol is not locked")
    missing_lock_values = sorted(
        field for field in REQUIRED_LOCK_FIELDS if not lock.get(field)
    )
    if missing_lock_values:
        errors.append(
            "protocol lock is incomplete: " + ", ".join(missing_lock_values)
        )
    fingerprint = lock.get("protocol_fingerprint")
    if not fingerprint or state.get("protocol_fingerprint") != fingerprint:
        errors.append("state and protocol fingerprints do not match")
    protocol_path = ROOT / "protocol.md"
    if lock.get("approved_protocol_sha256") != sha256(protocol_path):
        errors.append("protocol.md SHA-256 does not match the approved protocol")
    if lock.get("protocol_fingerprint") != lock.get("approved_protocol_sha256"):
        errors.append("protocol fingerprint does not match approved protocol SHA-256")
    if budget.get("status") != "active":
        errors.append("research budget is not active")
    if budget.get("exhausted"):
        errors.append("research budget is exhausted")
    limits = budget.get("limits", {})
    if limits.get("max_experiments") is None:
        errors.append("experiment-count budget is undefined")
    if limits.get("max_training_seconds") is None:
        errors.append("training-time budget is undefined")
    per_experiment_limit = limits.get("max_seconds_per_experiment")
    if per_experiment_limit is None:
        errors.append("per-experiment training time box is undefined")
    if experiment_id != state.get("next_experiment_id"):
        errors.append("experiment ID does not match state.next_experiment_id")
    if not experiment_id.startswith("ZN-E"):
        errors.append("ZN experiment IDs must start with 'ZN-E'")
    if experiment_id in recorded_ids():
        errors.append("experiment ID already exists in results.tsv")
    pending_rows = [
        row.get("experiment", "unknown")
        for row in result_rows()
        if not row.get("decision")
        or row.get("conclusion") == "Awaiting reviewed conclusion."
    ]
    if pending_rows:
        errors.append(
            "previous results still need analysis: " + ", ".join(pending_rows)
        )
    if (ROOT / "experiments" / experiment_id).exists():
        errors.append("experiment snapshot directory already exists")

    if not proposal_path.exists():
        errors.append(f"proposal file does not exist: {proposal_path}")
    else:
        with proposal_path.open() as handle:
            proposal = json.load(handle)
        missing = sorted(REQUIRED_PROPOSAL_FIELDS - proposal.keys())
        if missing:
            errors.append("proposal is missing fields: " + ", ".join(missing))
        if proposal.get("id") != experiment_id:
            errors.append("proposal ID does not match requested experiment ID")
        if proposal.get("phase") not in VALID_PHASES:
            errors.append("proposal has an invalid phase")
        elif proposal.get("phase") != state.get("phase"):
            errors.append("proposal phase does not match state.phase")
        if proposal.get("training_stage") not in VALID_TRAINING_STAGES:
            errors.append("proposal has an invalid training_stage")
        if proposal.get("type") not in EXPERIMENT_TYPES:
            errors.append("proposal has an invalid experiment type")
        elif proposal.get("phase") == "phase1_model" and proposal.get("type") not in {
            "baseline",
            "architecture",
            "ablation",
        }:
            errors.append("Phase 1 permits only baseline, architecture, or ablation runs")
        elif proposal.get("phase") == "phase3_tinyml" and proposal.get("type") not in {
            "compression",
        }:
            errors.append("Phase 3 permits only compression runs")
        for field in (
            "question_inherited",
            "unresolved_from_reference",
            "why_this_experiment",
            "changed_components",
            "held_constant_components",
            "minimum_evidence_requirements",
        ):
            if not isinstance(proposal.get(field), str) or not proposal[field].strip():
                errors.append(f"proposal.{field} must be a non-empty string")
        model = proposal.get("model")
        if not isinstance(model, dict) or not model.get("display_name"):
            errors.append("proposal.model.display_name is required")
        reference = proposal.get("reference_experiment")
        if reference is not None and reference not in recorded_ids():
            errors.append("proposal.reference_experiment is not in results.tsv")
        estimated = proposal.get("estimated_budget")
        if not isinstance(estimated, dict):
            errors.append("estimated_budget must be an object")
        else:
            estimated_experiments = estimated.get("experiments")
            estimated_seconds = estimated.get("training_seconds")
            if estimated_experiments != 1:
                errors.append("estimated_budget.experiments must equal 1")
            if not isinstance(estimated_seconds, (int, float)) or estimated_seconds <= 0:
                errors.append("estimated_budget.training_seconds must be positive")
            elif (
                isinstance(per_experiment_limit, (int, float))
                and estimated_seconds > per_experiment_limit
            ):
                errors.append("estimated training time exceeds the per-experiment time box")
            remaining = budget.get("remaining", {})
            if (
                isinstance(estimated_experiments, (int, float))
                and isinstance(remaining.get("experiments"), (int, float))
                and estimated_experiments > remaining["experiments"]
            ):
                errors.append("estimated experiment count exceeds remaining budget")
            if (
                isinstance(estimated_seconds, (int, float))
                and isinstance(remaining.get("training_seconds"), (int, float))
                and estimated_seconds > remaining["training_seconds"]
            ):
                errors.append("estimated training time exceeds remaining budget")
        training = proposal.get("training")
        if not isinstance(training, dict):
            errors.append("proposal.training must be an object")
        else:
            minimum_epochs = training.get("minimum_epochs")
            maximum_epochs = training.get("max_epochs")
            if not isinstance(minimum_epochs, int) or minimum_epochs < 1:
                errors.append("training.minimum_epochs must be a positive integer")
            if not isinstance(maximum_epochs, int) or maximum_epochs < 1:
                errors.append("training.max_epochs must be a positive integer")
            elif isinstance(minimum_epochs, int) and maximum_epochs < minimum_epochs:
                errors.append("training.max_epochs must be >= minimum_epochs")
            if proposal.get("phase") == "phase3_tinyml":
                if training.get("main_loss") != "relative_huber":
                    errors.append("Phase 3 requires main_loss='relative_huber'")
                if training.get("relative_huber_delta") != 0.15:
                    errors.append("Phase 3 requires relative_huber_delta=0.15")
                if training.get("learning_rate") != lock["training_lock"]["learning_rate"]:
                    errors.append("Phase 3 requires the locked learning_rate (protocol_lock.training_lock)")
                if training.get("batch_size") != lock["training_lock"]["batch_size"]:
                    errors.append("Phase 3 requires the locked batch_size (protocol_lock.training_lock)")
                if training.get("physics_loss_weight", 0.0) != 0.0:
                    errors.append("Phase 3 requires physics_loss_weight=0")
                if training.get("physics2_type"):
                    errors.append("Phase 3 forbids physics2_type")
            if proposal.get("phase") == "phase2_loss":
                REGISTERED_PHASE2 = {
                    "whatif_fade_p0",
                    "whatif_fade_p0_flip",
                    "whatif_improve_p0u",
                    "whatif_fade_p0b",
                    "prefix_drift",
                    "degradation_law",
                    "degradation_law2",
                    "timewarp_aug",
                    "whatif_trend_p4",
                    "whatif_trend_p4b",
                    "whatif_knee_p0c",
                    "whatif_vsag_p2",
                    "whatif_improve_p0u",
                }
                if training.get("main_loss") != "relative_huber" or training.get("relative_huber_delta") != 0.15:
                    errors.append("Phase 2 keeps Relative Huber delta 0.15 as main loss")
                if training.get("learning_rate") != lock["training_lock"]["learning_rate"] or training.get("batch_size") != lock["training_lock"]["batch_size"]:
                    errors.append("Phase 2 keeps the locked Adam lr and batch size (protocol_lock.training_lock)")
                if training.get("scheduler") == "warmup_cosine" and (  # amendment A11
                    training.get("peak_learning_rate") != 5e-4 or training.get("warmup_epochs", 1) != 1
                ):
                    errors.append("A11 warmup_cosine requires peak_learning_rate 5e-4 and warmup_epochs 1")
                if training.get("physics_loss_weight", 0.0) != 0.0 and training.get("physics_type") not in REGISTERED_PHASE2:
                    errors.append("Phase 2 physics_type must be registered in loss.md §4.6")
                if training.get("physics2_type") and training.get("physics2_type") not in (REGISTERED_PHASE2 | {"budget_bound", "timewarp_aug", "stat_tail_balance"}):
                    errors.append("Phase 2 physics2_type must be registered in loss.md §4.6/4.7")
            if proposal.get("phase") == "phase3_tinyml":  # Phase 3 compression (T009): retrain smaller models under the Phase-1 objective
                if proposal.get("type") != "compression":
                    errors.append("Phase 3 permits only compression runs")
                if training.get("main_loss") != "relative_huber" or training.get("relative_huber_delta") != 0.15:
                    errors.append("Phase 3 keeps Relative Huber delta 0.15 as main loss")
                if training.get("learning_rate") != lock["training_lock"]["learning_rate"] or training.get("batch_size") != lock["training_lock"]["batch_size"]:
                    errors.append("Phase 3 keeps the locked Adam lr and batch size (protocol_lock.training_lock)")
                if training.get("physics_loss_weight", 0.0) != 0.0 or training.get("physics2_type"):
                    errors.append("Phase 3 compression runs use no physics or auxiliary loss")
            if proposal.get("phase") == "phase1_model":
                loss_lock = state.get("loss_lock", {})
                expected_loss_lock = {
                    "main_loss": "relative_huber",
                    "relative_huber_delta": 0.15,
                    "checkpoint_mape_tolerance": 0.005,
                    "physics_loss_weight": 0.0,
                    "extra_loss_weight": 0.0,
                }
                for key, expected in expected_loss_lock.items():
                    if loss_lock.get(key) != expected:
                        errors.append(
                            f"state.loss_lock.{key} must equal {expected!r}"
                        )
                if training.get("main_loss") != "relative_huber":
                    errors.append("Phase 1 requires main_loss='relative_huber'")
                if training.get("relative_huber_delta") != 0.15:
                    errors.append("Phase 1 requires relative_huber_delta=0.15")
                if training.get("physics_loss_weight", 0.0) != 0.0:
                    errors.append("Phase 1 requires physics_loss_weight=0")
                if training.get("extra_loss_weight", 0.0) != 0.0:
                    errors.append("Phase 1 requires extra_loss_weight=0")
                if training.get("prefix_consistency_weight", 0.0) != 0.0:
                    errors.append("Phase 1 forbids prefix-consistency loss")
                if training.get("target_transform", "standard") != "standard":
                    errors.append("Phase 1 requires standardized-EOL targets")
                if training.get("checkpoint_mape_tolerance") != 0.005:
                    errors.append("Phase 1 requires checkpoint_mape_tolerance=0.005")
                if training.get("learning_rate") != lock["training_lock"]["learning_rate"]:
                    errors.append("Phase 1 requires the locked learning_rate (protocol_lock.training_lock)")
                if training.get("batch_size") != lock["training_lock"]["batch_size"]:
                    errors.append("Phase 1 requires the locked batch_size (protocol_lock.training_lock)")
                if training.get("scheduler", "none") != "none":
                    errors.append("Phase 1 requires scheduler='none'")

    cache_value = lock.get("cache_path")
    cache_path = Path(cache_value) if cache_value else None
    if cache_path and not cache_path.is_absolute():
        cache_path = ROOT / cache_path
    if cache_path is None or not cache_path.exists():
        errors.append("locked cache does not exist")
    elif not lock.get("cache_sha256"):
        errors.append("locked cache has no SHA-256")
    elif sha256(cache_path) != lock["cache_sha256"]:
        errors.append("locked cache SHA-256 mismatch")

    if errors:
        raise RuntimeError("Preflight failed:\n- " + "\n- ".join(errors))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("experiment_id")
    parser.add_argument("proposal", type=Path)
    args = parser.parse_args()
    validate(args.experiment_id, args.proposal)
    print(f"Preflight passed for {args.experiment_id}")


if __name__ == "__main__":
    main()
