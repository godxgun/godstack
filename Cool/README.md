# Cool

Build server-side components with C! Use the single-header library as a standalone HTML formatter
or use the transpiler to convert `.cool` files into `.c` files that you can include directly!

**NOTE:** Run `./build` in the monorepo project root to compile the transpiler to `bin/`

## Example

Write something like so:

```code
COOL void Api(char *name, char *func, char *desc) {
    <div class="api-card">
        <h3 class="api-name">{ name }</h3>
        <code class="language-c">{ func } </code>
        <div class="api-description">
            { desc }
        </div>
    </div>
}
```

Call `cool_transpiler` to get `view.cool.c` and include it into your code.

```
./bin/cool_transpiler view.cool -o view.cool.c
```

```c
#include "views.cool.c" // include server side components directly

int main() {
    char func_name[256];
    char func_decl[256];
    char *comment_text = NULL;
    /* ... */
    /* call you view directly! */
    if (feel_like_it) {
        Api(func_name, func_decl, comment_text);
    }
    return 0;
}

```

## Contributing 

This is one of the few open source projects where I'm very open to contribution.
Web is not my area of expertise. And this idea deserves fome fleshing out.
