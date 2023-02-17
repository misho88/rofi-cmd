#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include <rofi/mode.h>
#include <rofi/mode-private.h>
#include <rofi/helper.h>

extern void rofi_view_reload(void); /* forces Rofi to refresh itself */


int          cmd_init             (      Mode * mode);
void         cmd_destroy          (      Mode * mode);
char *       cmd_get_message      (const Mode * mode);
unsigned int cmd_get_num_entries  (const Mode * mode);
char *       cmd_get_display_value(const Mode * mode, unsigned int selected_line, int * state, GList ** attribute_list, int get_entry);
char *       cmd_preprocess_input (      Mode * mode, const char * input);
int          cmd_token_match      (const Mode * mode, rofi_int_matcher ** tokens, unsigned int index);
ModeMode     cmd_mode_result      (      Mode * mode, int menu_retv, char ** input, unsigned int selected_line);
void         cmd_run_subprocess   (      Mode * mode, const char * input);


Mode mode = {
	.abi_version        = ABI_VERSION,
	.name               = "cmd",
	.cfg_name_key       = "display-cmd",  /* what is this? */

	._init              = cmd_init,
	._destroy           = cmd_destroy,

	._get_message       = cmd_get_message,
	._get_num_entries   = cmd_get_num_entries,
	._get_display_value = cmd_get_display_value,

	._preprocess_input  = cmd_preprocess_input,
	._token_match       = cmd_token_match,
	._result            = cmd_mode_result,
};

struct cmd_data {
	char * format;
	char * result;
	char * input;
	char * command;
	char * stdout;
	char * stderr;
	int status;
	int use_stdin;
	int n_lines;
	char ** lines;
};

int
cmd_contains_directive(const char * format)
{
	if (format[0] == '\0') return 0;
	if (format[0] == '%') return format[1] != '%';
	return cmd_contains_directive(format + 1);
}

int
cmd_init(Mode * mode)
{
	struct cmd_data * data = mode_get_private_data(mode);
	if (data != NULL) return TRUE; /* if already set; FIXME: should this be an error? */

	data = malloc(sizeof(struct cmd_data));
	*data = (struct cmd_data){
		.format = "echo %s",
		.result = "cat",
		.status = 0,
	};

	if (find_arg("-cmd-format") >= 0) find_arg_str("-cmd-format", &data->format);
	if (find_arg("-cmd-result") >= 0) find_arg_str("-cmd-result", &data->result);

	data->use_stdin = !cmd_contains_directive(data->format);

	mode_set_private_data(mode, (void *)data);

	cmd_run_subprocess(mode, "");

	return TRUE;
}

void
cmd_destroy(Mode * mode)
{
	struct cmd_data * data = mode_get_private_data(mode);
	if (data == NULL) return;  /* if already freed; FIXME: should this be an error? */
	if (data->command) free(data->command);
	if (data->stdout ) free(data->stdout );
	if (data->stderr ) free(data->stderr );
	if (data->lines  ) free(data->lines  );
	free(data);
	mode_set_private_data(mode, NULL);
}

char *
cmd_get_message(const Mode * mode)
{
	struct cmd_data * data = mode_get_private_data(mode);
	const char * fmt = data->status == 0 ? "%s" : "<span foreground='Red'>%s</span>";
	return g_markup_printf_escaped(fmt, data->stderr);
}

unsigned int
cmd_get_num_entries(const Mode * mode)
{
	struct cmd_data * data = mode_get_private_data(mode);
	return 1 + data->n_lines;
}

char *
cmd_get_display_value(const Mode * mode, unsigned int selected_line, int * state, GList ** attribute_list, int get_entry)
{
	if (get_entry == 0) return NULL; /* I have never seen this not be 1 */
	struct cmd_data * data = mode_get_private_data(mode);

	if (selected_line == 0) return g_strdup(data->command);

	int i = selected_line - 1;
	int len = data->lines[i + 1] - data->lines[i] - 1;
	return g_strdup_printf("%.*s", len, data->lines[i]);
}

void
cmd_subprocess_callback(GSubprocess * subprocess, GAsyncResult * result, Mode * mode)
{
	GError * error = NULL;

	struct cmd_data * data = mode_get_private_data(mode);


	typeof(&g_subprocess_get_stdout_pipe) get_pipes[2] = { g_subprocess_get_stdout_pipe, g_subprocess_get_stderr_pipe };
	char ** destinations[2] = { &data->stdout, &data->stderr };
	char * names[2] = { "stdout", "stderr" };

	for (int i = 0; i < 2; i++) {
		GInputStream * stream = get_pipes[i](subprocess);
		char buffer[4096];
		gsize n_read;
		g_input_stream_read_all(stream, buffer, sizeof(buffer) - 1, &n_read, NULL, &error);
		buffer[n_read] = '\0';
		if (error) {
			g_error("'%s' failed to read %s: %s", data->command, names[i], error->message);
			g_error_free(error);
		}
		g_input_stream_close(stream, NULL, &error);
		if (error) {
			g_error("'%s' failed to close %s: %s", data->command, names[i], error->message);
			g_error_free(error);
		}
		free(*destinations[i]);
		*destinations[i] = g_strdup(buffer);
	}

	g_subprocess_wait_finish(subprocess, result, &error);
	if (error) {
		g_error("'%s' failed to wait on: %s", data->command, error->message);
		g_error_free(error);
	}

	data->status = g_subprocess_get_exit_status(subprocess);

	data->n_lines = 1;
	for (char * c = data->stdout; *c != '\0'; c++)
		data->n_lines += *c == '\n';
	data->lines = realloc(data->lines, (data->n_lines + 1) * sizeof(char **));

	char * c = data->stdout;
	int i = 0;
	data->lines[i++] = c;
	for (; *c != '\0'; c++) {
		if (*c == '\n') {
			data->lines[i++] = c + 1;
		}
	}
	data->lines[i++] = c;
	rofi_view_reload();
}

void
cmd_run_subprocess(Mode * mode, const char * input)
{
	struct cmd_data * data = mode_get_private_data(mode);

	free(data->input);
	data->input = strdup(input);

	GError * error = NULL;

	GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE;

	free(data->command);
	if (data->use_stdin) {
		flags |= G_SUBPROCESS_FLAGS_STDIN_PIPE;
		data->command = g_strdup(data->format);
	}
	else {
		data->command = g_strdup_printf(data->format, input);

	}
	GSubprocess * subprocess = g_subprocess_new(flags, &error, "sh", "-c", data->command, NULL);
	if (error) {
		g_error("'%s' failed to start: %s", data->command, error->message);
		g_error_free(error);
	}

	if (data->use_stdin) {
		GOutputStream * stream = g_subprocess_get_stdin_pipe(subprocess);
		g_output_stream_write_all(stream, input, strlen(input), NULL, NULL, &error);
		if (error) {
			g_error("'%s' failed to write %s: %s", data->command, "stdin", error->message);
			g_error_free(error);
		}
		g_output_stream_close(stream, NULL, &error);
		if (error) {
			g_error("'%s' failed to close %s: %s", data->command, "stdin", error->message);
			g_error_free(error);
		}
	}

	g_subprocess_wait_async(subprocess, NULL, (GAsyncReadyCallback)cmd_subprocess_callback, mode);
}

char *
cmd_preprocess_input(Mode * mode, const char * input)
{
	struct cmd_data * data = mode_get_private_data(mode);

	if (data->input && strcmp(input, data->input) == 0)
		return strdup(input);

	rofi_view_reload();
	cmd_run_subprocess(mode, input);

	return strdup(input);
}

int
cmd_token_match(const Mode * mode, rofi_int_matcher ** tokens, unsigned int index)
{
	return TRUE;
}

ModeMode
cmd_mode_result(Mode * mode, int menu_retv, char ** input, unsigned int selected_line)
{
	struct cmd_data * data = mode_get_private_data(mode);

	if (data->stdout == NULL || selected_line == (unsigned int)-1)
		return MODE_EXIT;

	char * result = selected_line == 0 ? data->stdout : data->lines[selected_line - 1];
	int len = selected_line == 0 ? strlen(result) : data->lines[selected_line] - result - 1;

	int use_stdin = !cmd_contains_directive(data->result);

	GError * error = NULL;
	GSubprocess * subprocess;
	if (use_stdin) {
		const GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDIN_PIPE;
		subprocess = g_subprocess_new(flags, &error, "sh", "-c", data->result, NULL);
		if (error) {
			g_error("'%s' failed to start: %s", data->result, error->message);
			g_error_free(error);
		}
		GOutputStream * stream = g_subprocess_get_stdin_pipe(subprocess);
		g_output_stream_write_all(stream, result, len, NULL, NULL, &error);
		if (error) {
			g_error("'%s' failed to write %s: %s", data->result, "stdin", error->message);
			g_error_free(error);
		}
		g_output_stream_close(stream, NULL, &error);
		if (error) {
			g_error("'%s' failed to close %s: %s", data->result, "stdin", error->message);
			g_error_free(error);
		}
	}
	else {
		const GSubprocessFlags flags = G_SUBPROCESS_FLAGS_NONE;
		char * tmp = g_strdup_printf("%.*s", len, result);
		char * command = g_strdup_printf(data->result, tmp);
		g_free(tmp);
		subprocess = g_subprocess_new(flags, &error, "sh", "-c", command, NULL);
		g_free(command);
		if (error) {
			g_error("'%s' failed to start: %s", command, error->message);
			g_error_free(error);
		}
	}

	g_subprocess_wait(subprocess, NULL, &error);
	if (error) {
		g_error("'%s' failed to wait on: %s", data->result, error->message);
		g_error_free(error);
	}

	int status = g_subprocess_get_exit_status(subprocess);

	if (status != 0) exit(status);

	return MODE_EXIT;
}
