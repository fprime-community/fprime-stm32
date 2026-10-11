from fprime_stm32.project_wiring import (
    ensure_deployment_registered,
    patch_deployment_cmakelists,
    patch_namespace_cmakelists,
    patch_root_cmakelists,
    patch_top_cmakelists,
)

ROOT_CMAKELISTS = """cmake_minimum_required(VERSION 3.24.2)
project(Stm32h7Project C CXX)

include("${CMAKE_CURRENT_LIST_DIR}/lib/fprime/cmake/FPrime.cmake")
fprime_setup_included_code()

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32h7Project")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32h7Project/Deployments/Stm32h7Deployment/")
"""

NAMESPACE_CMAKELISTS = """# This CMake file is intended to register project-wide objects.

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components")
"""

DEPLOYMENT_CMAKELISTS = """add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")
# Add custom components to this specific deployment here
# add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/MyComponent/")
register_fprime_deployment(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
    DEPENDS
        ${FPRIME_CURRENT_MODULE}_Top
)
"""

TOP_CMAKELISTS = """register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/topology.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Stm32h7DeploymentTopology.cpp"
    DEPENDS
        Fw_Logger
)
"""

# Reproduces the real bug: `fprime-util new --deployment`'s own prompt appended the deployment
# registration to the NAMESPACE CMakeLists.txt with a path that repeats the namespace name -
# valid only from ROOT context, so CMake can't find the (nonexistent) doubly-nested directory.
ROOT_CMAKELISTS_MISSING_DEPLOYMENT = """cmake_minimum_required(VERSION 3.24.2)
project(Stm32h7Project C CXX)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32h7Project")
"""

NAMESPACE_CMAKELISTS_WITH_BROKEN_REGISTRATION = """add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32h7Project/Deployments/Stm32h7Deployment/")
"""

# Reproduces a second real bug, hit on a from-scratch project: `fprime-util new --deployment`
# registered the deployment with a CORRECT path, but directly in the namespace CMakeLists.txt,
# positioned before Hardware/ would later be added there. fprime's CMake API requires a
# deployment to be registered after the targets (FprimeStm32) it depends on, so this fails at
# configure time with "'FprimeStm32' must be defined before ... deployment" even though the path
# itself is fine - it's the file/ordering that's wrong, not the path.
NAMESPACE_CMAKELISTS_WITH_CORRECTLY_PATHED_REGISTRATION = """add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Deployments/Stm32h7Deployment/")
"""


def test_patch_root_cmakelists_enables_asm_and_adds_config():
    patched, actions = patch_root_cmakelists(ROOT_CMAKELISTS)

    assert "enable_language(ASM)" in patched
    assert patched.index("enable_language(ASM)") > patched.index("project(Stm32h7Project")
    assert 'add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config")' in patched
    assert len(actions) == 2


def test_patch_root_cmakelists_idempotent():
    once, _ = patch_root_cmakelists(ROOT_CMAKELISTS)
    twice, actions = patch_root_cmakelists(once)

    assert once == twice
    assert actions == []


def test_patch_namespace_cmakelists_adds_hardware():
    patched, actions = patch_namespace_cmakelists(NAMESPACE_CMAKELISTS)

    assert 'add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Hardware")' in patched
    assert len(actions) == 1


def test_patch_namespace_cmakelists_idempotent():
    once, _ = patch_namespace_cmakelists(NAMESPACE_CMAKELISTS)
    twice, actions = patch_namespace_cmakelists(once)

    assert once == twice
    assert actions == []


def test_patch_deployment_cmakelists_full_wiring():
    patched, actions = patch_deployment_cmakelists(DEPLOYMENT_CMAKELISTS)

    assert "restrict_platforms(stm32h7)" in patched
    assert "${FPRIME_STM32_STARTUP_SOURCE}" in patched
    assert "${FPRIME_STM32_IT_SOURCE}" in patched
    for dep in ("FprimeStm32", "FprimeStm32Config", "FprimeStm32Allocator", "Os_Baremetal_OverrideNewDelete"):
        assert dep in patched
    assert "target_link_options(${FPRIME_CURRENT_MODULE} PRIVATE" in patched
    assert "-T${FPRIME_STM32_LINKER_SCRIPT}" in patched
    assert "--specs=nosys.specs" in patched
    assert len(actions) >= 6

    # SOURCES/DEPENDS insertions land inside the original call, before its closing paren
    call_start = patched.index("register_fprime_deployment(")
    call_end = patched.index(")", call_start)
    assert "${FPRIME_STM32_STARTUP_SOURCE}" in patched[call_start:call_end]
    assert "FprimeStm32Allocator" in patched[call_start:call_end]


def test_patch_deployment_cmakelists_idempotent():
    once, _ = patch_deployment_cmakelists(DEPLOYMENT_CMAKELISTS)
    twice, actions = patch_deployment_cmakelists(once)

    assert once == twice
    assert actions == []


def test_patch_top_cmakelists_adds_allocator_dependency():
    patched, actions = patch_top_cmakelists(TOP_CMAKELISTS)

    assert "FprimeStm32Allocator" in patched
    call_start = patched.index("register_fprime_module(")
    call_end = patched.index(")", call_start)
    assert "FprimeStm32Allocator" in patched[call_start:call_end]
    assert len(actions) == 2


def test_patch_top_cmakelists_idempotent():
    once, _ = patch_top_cmakelists(TOP_CMAKELISTS)
    twice, actions = patch_top_cmakelists(once)

    assert once == twice
    assert actions == []


def test_ensure_deployment_registered_fixes_broken_namespace_registration():
    root, namespace, root_actions, namespace_actions = ensure_deployment_registered(
        ROOT_CMAKELISTS_MISSING_DEPLOYMENT,
        NAMESPACE_CMAKELISTS_WITH_BROKEN_REGISTRATION,
        "Stm32h7Project",
        "Stm32h7Deployment",
    )

    # the broken, doubly-nested line is gone from the namespace file
    assert "Stm32h7Project/Deployments/Stm32h7Deployment" not in namespace
    assert len(namespace_actions) == 1

    # a correct registration now exists in root instead
    assert 'add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32h7Project/Deployments/Stm32h7Deployment/")' in root
    assert len(root_actions) == 1


def test_ensure_deployment_registered_moves_correctly_pathed_namespace_registration():
    root, namespace, root_actions, namespace_actions = ensure_deployment_registered(
        ROOT_CMAKELISTS_MISSING_DEPLOYMENT,
        NAMESPACE_CMAKELISTS_WITH_CORRECTLY_PATHED_REGISTRATION,
        "Stm32h7Project",
        "Stm32h7Deployment",
    )

    # the deployment line is gone from the namespace file, even though its path was fine
    assert "Deployments/Stm32h7Deployment" not in namespace
    assert len(namespace_actions) == 1

    # a correct registration now exists in root instead
    assert 'add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Stm32h7Project/Deployments/Stm32h7Deployment/")' in root
    assert len(root_actions) == 1


def test_ensure_deployment_registered_noop_when_already_correct():
    root, namespace, root_actions, namespace_actions = ensure_deployment_registered(
        ROOT_CMAKELISTS, NAMESPACE_CMAKELISTS, "Stm32h7Project", "Stm32h7Deployment"
    )

    assert root == ROOT_CMAKELISTS
    assert namespace == NAMESPACE_CMAKELISTS
    assert root_actions == []
    assert namespace_actions == []


def test_ensure_deployment_registered_idempotent():
    once_root, once_namespace, _, _ = ensure_deployment_registered(
        ROOT_CMAKELISTS_MISSING_DEPLOYMENT,
        NAMESPACE_CMAKELISTS_WITH_BROKEN_REGISTRATION,
        "Stm32h7Project",
        "Stm32h7Deployment",
    )
    twice_root, twice_namespace, root_actions, namespace_actions = ensure_deployment_registered(
        once_root, once_namespace, "Stm32h7Project", "Stm32h7Deployment"
    )

    assert once_root == twice_root
    assert once_namespace == twice_namespace
    assert root_actions == []
    assert namespace_actions == []


def test_patch_deployment_cmakelists_targets_requested_platform():
    patched, actions = patch_deployment_cmakelists(DEPLOYMENT_CMAKELISTS, "stm32f4")

    assert "restrict_platforms(stm32f4)" in patched
    assert "stm32h7" not in patched
    assert any("stm32f4 platform" in a for a in actions)


def test_patch_deployment_cmakelists_retargets_existing_restriction():
    h7, _ = patch_deployment_cmakelists(DEPLOYMENT_CMAKELISTS, "stm32h7")
    f4, actions = patch_deployment_cmakelists(h7, "stm32f4")

    assert "restrict_platforms(stm32f4)" in f4
    assert "restrict_platforms(stm32h7)" not in f4
    assert any("Changed restrict_platforms(stm32h7)" in a for a in actions)
