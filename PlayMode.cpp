#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

glm::vec3 player_pos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 player_vel = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec2 player_angle = glm::vec2(0.0f, 0.0f);
float tile_size = 10;
float platform_size = 10 * 4;
bool launchProcessed = false;
bool launched = false;
bool preview = false;
float previewTime = 0.0f;
float blastoffTime = 0.0f;
float initialPreviewTime = 3.0f;

Scene::Transform *player_transform = new Scene::Transform();

const uint8_t grass = 1;
const uint8_t antimatter = 2;
const uint8_t target = 3;
const uint8_t field_z1 = 4;
const uint8_t field_z2 = 5;
const uint8_t level_size = 32;
const uint8_t level_height = 16;

struct Level
{
    uint8_t blocks[level_size][level_size][level_height];
    uint8_t spawn[2];

    Level(uint8_t x, uint8_t y)
    {
        spawn[0] = x;
        spawn[1] = y;

        for (int i = 0; i < level_size; i++)
        {
            for (int j = 0; j < level_size; j++)
            {
                for (int k = 0; k < level_height; k++)
                {
                    blocks[i][j][k] = 0;
                }
            }
        }
    }
};

Level current_level = Level(16, 16);

uint8_t get_block_at(Level &l, float x, float y, float z)
{
    int bx = (int) floor(x / tile_size) + l.spawn[0];
    int by = (int) floor(y / tile_size) + l.spawn[1];
    int bz = (int) floor(z / tile_size) + level_height / 2;

    if (bx < -2 || bx >= level_size + 2 || by < -2 || by >= level_size + 2 || bz < -2 || bz >= level_height + 2)
        return antimatter;
    else if (bx < 0 || bx >= level_size || by < 0 || by >= level_size || bz < 0 || bz >= level_height)
        return 0;
    else
    {
        return l.blocks[bx][by][bz];
    }
}

GLuint grass_vao = 0;
Load< MeshBuffer > grass_mesh(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("data/grass.pnct"));
    grass_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

GLuint antimatter_vao = 0;
Load< MeshBuffer > antimatter_mesh(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("data/block.pnct"));
    antimatter_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

GLuint target_vao = 0;
Load< MeshBuffer > target_mesh(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("data/target.pnct"));
    target_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

GLuint field_vao = 0;
Load< MeshBuffer > field_mesh(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("data/field.pnct"));
    field_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

GLuint arrow_vao = 0;
Load< MeshBuffer > arrow_mesh(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("data/arrow.pnct"));
    arrow_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

GLuint astronaut_vao = 0;
Load< MeshBuffer > astronaut_mesh(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("data/astronaut.pnct"));
    astronaut_vao = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

Load< Scene > empty_scene(LoadTagDefault, []() -> Scene const * {
	Scene* s = new Scene(data_path("data/emptyscene.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){});
    return s;
});

Mesh get_mesh_from_id(uint8_t id)
{
    if (id == grass)
        return grass_mesh->lookup("grass.001");
    else if (id == antimatter)
        return antimatter_mesh->lookup("block");
    else if (id == target)
        return target_mesh->lookup("target");
    else
        return field_mesh->lookup("field");
}

GLuint get_vao_from_id(uint8_t id)
{
    if (id == grass)
        return grass_vao;
    else if (id == antimatter)
        return antimatter_vao;
    else if (id == target)
        return target_vao;
    else
        return field_vao;
}

void add_arrow_to_scene(Scene &scene, uint8_t dir, int8_t x, int8_t y, int8_t z)
{
    Mesh const &mesh = arrow_mesh->lookup("arrow");

    Scene::Transform *t = new Scene::Transform();
    t->position.x = (x + 1) * tile_size;
    t->position.y = (y) * tile_size;
    t->position.z = (z + 1) * tile_size;

    if (dir == 0)
        t->rotation = glm::normalize(
            glm::angleAxis(0.0f, glm::vec3(0.0f, 0.0f, 1.0f))
            * glm::angleAxis((float)(M_PI), glm::vec3(1.0f, 0.0f, 0.0f)));
    else if (dir == 1)
        t->rotation = glm::normalize(
                glm::angleAxis(0.0f, glm::vec3(0.0f, 0.0f, 1.0f))
                * glm::angleAxis((float)(0), glm::vec3(1.0f, 0.0f, 0.0f)));

    t->scale = glm::vec3(tile_size / 10, tile_size / 10, tile_size / 10);
    scene.drawables.emplace_back(t);
    Scene::Drawable &drawable = scene.drawables.back();

    drawable.pipeline = lit_color_texture_program_pipeline;

    drawable.pipeline.vao = arrow_vao;
    drawable.pipeline.type = mesh.type;
    drawable.pipeline.start = mesh.start;
    drawable.pipeline.count = mesh.count;
}

void add_block_to_scene(Scene &scene, uint8_t id, int8_t x, int8_t y, int8_t z)
{
    Mesh const &mesh = get_mesh_from_id(id);

    Scene::Transform *t = new Scene::Transform();
    t->position.x = (x + 0.5f) * tile_size;
    t->position.y = (y + 0.5f) * tile_size;
    t->position.z = (z + 0.5f) * tile_size;

    t->scale = glm::vec3(tile_size / 2, tile_size / 2, tile_size / 2);
    scene.drawables.emplace_back(t);
    Scene::Drawable &drawable = scene.drawables.back();

    drawable.pipeline = lit_color_texture_program_pipeline;

    drawable.pipeline.vao = get_vao_from_id(id);
    drawable.pipeline.type = mesh.type;
    drawable.pipeline.start = mesh.start;
    drawable.pipeline.count = mesh.count;

    if (id >= field_z1)
        add_arrow_to_scene(scene, id - field_z1, x, y, z);
}

void add_player_to_scene(Scene &scene)
{
    Mesh const &mesh = astronaut_mesh->lookup("astronaut");

    Scene::Transform *t = player_transform;
    t->scale = glm::vec3(tile_size / 4, tile_size / 4, tile_size / 4);
    scene.drawables.emplace_back(t);
    Scene::Drawable &drawable = scene.drawables.back();

    drawable.pipeline = lit_color_texture_program_pipeline;

    drawable.pipeline.vao = astronaut_vao;
    drawable.pipeline.type = mesh.type;
    drawable.pipeline.start = mesh.start;
    drawable.pipeline.count = mesh.count;
}

void load_level(Scene &scene, std::string file)
{
    scene.drawables.clear();
    add_player_to_scene(scene);

    current_level = Level(8, 16);

    glm::uvec2 size;
    std::vector< glm::u8vec4 > data;
    load_png(file, &size, &data, UpperLeftOrigin);

    for (int i = 0; i < size.x; i++)
    {
        for (int j = 0; j < size.y; j++)
        {
            uint8_t layer = (uint8_t) ((i / level_size) + (size.x / level_size) * (j / level_size));
            uint8_t x = (uint8_t) (i % level_size);
            uint8_t y = (uint8_t) (j % level_size);

            glm::vec4 col = data[i + j * size.x];
            uint8_t tile = 0;

            if (col.x == 255 && col.y == 0 && col.z == 0)
                tile = 2;
            else if (col.x == 0 && col.y == 255 && col.z == 0)
                tile = 1;
            else if (col.x == 255 && col.y == 255 && col.z == 0)
                tile = 3;
            else if (col.z == 255 && col.g == 0)
                tile = col.x + 4;

            if (tile != 0)
            {
                add_block_to_scene(scene, tile, x - current_level.spawn[0], y - current_level.spawn[1], layer - level_height / 2);
                current_level.blocks[x][y][layer] = tile;
            }
        }
    }
}

PlayMode::PlayMode() : scene(*empty_scene)
{
    load_level(scene, "data/level1.png");

    //get pointer to camera for convenience:
	if (scene.cameras.size() != 1)
        throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));

    camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

float clamp(float min, float num, float max)
{
    if (num < min)
        return min;
    else if (num > max)
        return max;
    else return num;
}


bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
            launched = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_RETURN) {
            preview = true;
            return true;
        }
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RETURN) {
            preview = false;
            return true;
        }
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				-evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
            player_angle += motion;
            player_angle.y = clamp((float)(-M_PI / 2.0f), player_angle.y, (float)(M_PI / 2.0f));
			return true;
		}
	}

	return false;
}

float bound_angle(float angle)
{
    while (angle > M_PI * 2)
    {
        angle -= M_PI * 2;
    }

    while (angle < 0)
    {
        angle += M_PI * 2;
    }

    return angle;
}

int level_index = 1;
void reset(Scene &scene, bool target)
{
    blastoffTime = 0.0f;
    launched = false;
    launchProcessed = false;
    player_pos = glm::vec3(0.0f, 0.0f, 0.0f);
    player_vel = glm::vec3(0.0f, 0.0f, 0.0f);

    if (target)
    {
        level_index++;
        initialPreviewTime = 3.0f;
        load_level(scene, "data/level" + std::to_string(level_index) + ".png");
    }
}

void PlayMode::update(float elapsed)
{
	{
        camera->fovy = 70;

        if (player_angle.x > M_PI * 2)
            player_angle.x -= (float) (M_PI * 2);

        if (player_angle.x < 0)
            player_angle.x += (float) (M_PI * 2);

        if (launched && !launchProcessed)
        {
            launchProcessed = true;
            constexpr float launchSpeed = 75.0f;

            float ay = player_angle.y;
            player_vel.x = -sin(player_angle.x) * cos(ay) * launchSpeed;
            player_vel.y = cos(player_angle.x) * cos(ay) * launchSpeed;
            player_vel.z = sin(ay) * launchSpeed;
        }

        if (!launched)
        {
            blastoffTime = clamp(0.0f, blastoffTime - elapsed * 2.0f, 1.0f);

            //combine inputs into a move:
            constexpr float PlayerSpeed = 30.0f;
            glm::vec2 move = glm::vec2(0.0f);
            if (left.pressed && !right.pressed) move.x = -1.0f;
            if (!left.pressed && right.pressed) move.x = 1.0f;
            if (down.pressed && !up.pressed) move.y = -1.0f;
            if (!down.pressed && up.pressed) move.y = 1.0f;

            //make it so that moving diagonally doesn't go faster:
            if (move != glm::vec2(0.0f))
                move = glm::normalize(move) * PlayerSpeed * elapsed;

            player_pos.x += move.x * cos(player_angle.x) - move.y * sin(player_angle.x);
            player_pos.y += move.x * sin(player_angle.x) + move.y * cos(player_angle.x);
            player_pos.x = clamp(-platform_size / 2, player_pos.x, platform_size / 2);
            player_pos.y = clamp(-platform_size / 2, player_pos.y, platform_size / 2);
            camera->transform->rotation = glm::normalize(
                glm::angleAxis(player_angle.x, glm::vec3(0.0f, 0.0f, 1.0f))
				* glm::angleAxis(player_angle.y, glm::vec3(1.0f, 0.0f, 0.0f))
                * glm::angleAxis(float(M_PI) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f))
			);

            player_transform->rotation = glm::normalize(
                    glm::angleAxis(player_angle.x + (float) M_PI, glm::vec3(0.0f, 0.0f, 1.0f))
            );
        }
        else
        {
            float a1 = atan2(player_vel.y, player_vel.x);
            float a2 = atan2(player_vel.z, sqrt(player_vel.x * player_vel.x + player_vel.y * player_vel.y));

            constexpr float gravity = 150.0f;

            blastoffTime = clamp(0.0f, blastoffTime + elapsed * 2.0f, 1.0f);
            camera->transform->rotation = glm::normalize(
                    glm::angleAxis(player_angle.x * (1.0f - blastoffTime) + blastoffTime * bound_angle(a1 - (float)(M_PI / 2)), glm::vec3(0.0f, 0.0f, 1.0f))
                    * glm::angleAxis(player_angle.y * (1.0f - blastoffTime) + blastoffTime * (float)(-M_PI / 2), glm::vec3(1.0f, 0.0f, 0.0f))
                    * glm::angleAxis(float(M_PI) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f))
            );

            player_pos += player_vel * elapsed;
            player_transform->rotation = player_transform->rotation * (1.0f - blastoffTime) + blastoffTime *
                glm::normalize(
                      glm::angleAxis(bound_angle(a1 + (float)(M_PI / 2)), glm::vec3(0.0f, 0.0f, 1.0f))
                      * glm::angleAxis(a2 * (1.0f - blastoffTime) + blastoffTime * (float)(-M_PI / 2), glm::vec3(1.0f, 0.0f, 0.0f))
                      * glm::angleAxis(float(M_PI) / 2.0f * (1.0f + blastoffTime), glm::vec3(1.0f, 0.0f, 0.0f)));

            uint8_t block = get_block_at(current_level, player_pos.x, player_pos.y, player_pos.z + 1.5f * tile_size);
            if (block == antimatter)
                reset(scene, false);
            else if (block == target)
                reset(scene, true);
            else if (block == field_z1)
                player_vel.z -= gravity * elapsed;
            else if (block == field_z2)
                player_vel.z += gravity * elapsed;
        }

        player_transform->position = player_pos;
        player_transform->position.z += blastoffTime * tile_size * 1.5f;

		camera->transform->position = player_pos;
        camera->transform->position.z += tile_size * 1.5;
        camera->transform->position.z += blastoffTime * tile_size * 10;

        if (preview)
            previewTime = clamp(0.0f, previewTime + elapsed * 2.5f, 1.0f);
        else
            previewTime = clamp(0.0f, previewTime - elapsed * 2.5f, 1.0f);

        if (initialPreviewTime > 0.0f)
        {
            previewTime = initialPreviewTime / 2.0f;

            if (previewTime > 1.0f)
                previewTime = 1.0f;

            initialPreviewTime -= elapsed;
        }

        camera->transform->position = camera->transform->position * (1.0f - previewTime) +
                                      previewTime * glm::vec3(0.0 - (current_level.spawn[0] - 16) * tile_size, -150.0 - (current_level.spawn[1] - 16) * tile_size, 150.0);
        camera->transform->rotation = glm::normalize(
                camera->transform->rotation * (1.0f - previewTime) +
                previewTime
                * glm::angleAxis(0.0f, glm::vec3(0.0f, 0.0f, 1.0f))
                * glm::angleAxis(float(-M_PI) / 4.0f, glm::vec3(1.0f, 0.0f, 0.0f))
                * glm::angleAxis(float(M_PI) / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f))
        );
    }

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);

    glm::vec3 light_dir = -glm::vec3(-sin(player_angle.x) * cos(player_angle.y), cos(player_angle.x) * cos(player_angle.y), sin(player_angle.y));
    glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(light_dir));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

    { //use DrawLines to overlay some text:
        glDisable(GL_DEPTH_TEST);
        float aspect = float(drawable_size.x) / float(drawable_size.y);
        DrawLines lines(glm::mat4(
                1.0f / aspect, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
        ));

        constexpr float H = 0.09f;
        float ofs = 2.0f / drawable_size.y;

        if (level_index < 6)
        {
            lines.draw_text("Move: WASD; Level overview: Enter; Blastoff: Space",
                            glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
                            glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                            glm::u8vec4(0x00, 0x00, 0x00, 0x00));
            lines.draw_text("Move: WASD; Level overview: Enter; Blastoff: Space",
                            glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
                            glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                            glm::u8vec4(0x50, 0x00, 0xff, 0x00));
        }
        else
        {
            lines.draw_text("Congratulations! You won!",
                            glm::vec3(-aspect / 2 + 0.0, 0.0, 0.0),
                            glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                            glm::u8vec4(0x50, 0x00, 0xff, 0x00));

            lines.draw_text("Congratulations! You won!",
                            glm::vec3(-aspect / 2 + 0.0 + ofs, 0.0 + ofs, 0.0),
                            glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                            glm::u8vec4(0x50, 0x00, 0xff, 0x00));
        }
    }
}
