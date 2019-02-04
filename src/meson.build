subinspector_src = files(
    'SubInspector.c'
)

# In practice, this is just never built statically.
add_project_arguments('-DBUILD_DLL', language: 'c')
subinspector = shared_library('SubInspector', subinspector_src, dependencies: deps)
